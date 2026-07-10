#include "scheduler.h"
#include "utils/hilos.h"
#include "utils/conexion.h"
#include <string.h>
#include <unistd.h>

//colas y listas para cada estado.
t_queue** colas_ready;
t_list* lista_block;
t_list* lista_exec;
t_list** listas_susp_ready;
t_list** listas_susp_block;

//mutexes para cada lista.
pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_block;
pthread_mutex_t mutex_exec;
pthread_mutex_t mutex_susp_ready;
pthread_mutex_t mutex_susp_block;

//semáforo productor-consumidor de colas_ready.
sem_t sem_procesos_en_ready;

//lista de pares (cpu,pid).
static t_list* lista_cpu_proceso;
static pthread_mutex_t mutex_cpu_proceso;

//estructura "estática". No poner en el ".h".
typedef struct {
    int socket_cpu;
    uint32_t pid;
} t_cpu_proceso;


static void inicializar_colas_ready() {
    colas_ready = malloc(sizeof(t_queue*) * cant_prioridades);
    for (int i = 0; i < cant_prioridades; i++) {
        colas_ready[i] = queue_create();
    }
}

static void inicializar_listas_susp_ready() {
    listas_susp_ready = malloc(sizeof(t_list*) * cant_prioridades);
    for (int i = 0; i < cant_prioridades; i++) {
        listas_susp_ready[i] = list_create();
    }
}

static void inicializar_listas_susp_block() {
    listas_susp_block = malloc(sizeof(t_list*) * cant_prioridades);
    for (int i = 0; i < cant_prioridades; i++) {
        listas_susp_block[i] = list_create();
    }
}

void inicializar_ks_planificador() {
    //inicializo colas.
    inicializar_colas_ready();

    //inicializo listas.
    lista_block = list_create();
    lista_exec = list_create();
    inicializar_listas_susp_ready();
    inicializar_listas_susp_block();
    lista_cpu_proceso = list_create();

    //inicializo mutexes.
    pthread_mutex_init(&mutex_ready, NULL);
    pthread_mutex_init(&mutex_block, NULL);
    pthread_mutex_init(&mutex_exec, NULL);
    pthread_mutex_init(&mutex_susp_ready, NULL);
    pthread_mutex_init(&mutex_susp_block, NULL);
    pthread_mutex_init(&mutex_cpu_proceso, NULL);

    //inicializo semáforo productor-consumidor.
    sem_init(&sem_procesos_en_ready, 0, 0);
}

// ----------------------------- READY -----------------------------

int obtener_socket_cpu_de(uint32_t pid) {

    pthread_mutex_lock(&mutex_cpu_proceso);

    int resultado = -1;
    for (int i = 0; i < list_size(lista_cpu_proceso); i++) {
        t_cpu_proceso* entry = list_get(lista_cpu_proceso, i);
        if (entry->pid == pid) {
            resultado = entry->socket_cpu;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_cpu_proceso);

    return resultado;
}

int32_t obtener_pid_de_cpu(int socket_cpu) {
    pthread_mutex_lock(&mutex_cpu_proceso);
    int32_t resultado = -1;
    for (int i = 0; i < list_size(lista_cpu_proceso); i++) {
        t_cpu_proceso* entry = list_get(lista_cpu_proceso, i);
        if (entry->socket_cpu == socket_cpu) {
            resultado = (int32_t)entry->pid;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_cpu_proceso);
    return resultado;
}

void agregar_a_ready(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_ready);

    int nivel = (strcmp(algoritmo, "CMN") == 0) ? (int)proceso->prioridad : 0;

    if(nivel < cant_prioridades) {
        queue_push(colas_ready[nivel], proceso);
    } else {
        log_warning(logger, "El proceso %d tiene una prioridad implanificable", proceso->pid);
    }

    pthread_mutex_unlock(&mutex_ready);

    sem_post(&sem_procesos_en_ready); //avisa que hay un proceso en colas_ready.

    if(hay_desalojo_cmn) { //viene del main. Solo es true si el algoritmo de planificación es CMN y si el desalojo entre colas está habilitado.

        pthread_mutex_lock(&mutex_exec);

        for (int i = 0; i < list_size(lista_exec); i++) {
            t_pcb* en_ejecucion = list_get(lista_exec, i);
            if (en_ejecucion->prioridad > proceso->prioridad) { //mayor priordad => menor número.
                int socket_cpu_ejecutando = obtener_socket_cpu_de(en_ejecucion->pid);
                if (socket_cpu_ejecutando != -1) {
                    log_info(logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d", en_ejecucion->pid, en_ejecucion->prioridad, proceso->pid, proceso->prioridad);
                    marcar_interrupcion(socket_cpu_ejecutando); //usa el mecanismo de tick progress.
                }
            break;
            }
        }
        pthread_mutex_unlock(&mutex_exec);
    }
}

void agregar_al_principio_de_ready(t_pcb* proceso){ //cambia con herencia

    pthread_mutex_lock(&mutex_ready);

    int nivel = (strcmp(algoritmo, "CMN") == 0) ? (int)proceso->prioridad : 0;

    if(nivel < cant_prioridades) {
        t_queue* cola = colas_ready[nivel];
        list_add_in_index(cola->elements, 0, proceso);
    } else {
        log_warning(logger, "El proceso %d tiene una prioridad implanificable", proceso->pid);
    }

    pthread_mutex_unlock(&mutex_ready);

    sem_post(&sem_procesos_en_ready); //avisa que hay un proceso en colas_ready.
}

void reinsertar_al_principio_de_ready(t_pcb* proceso) {
    pthread_mutex_lock(&mutex_ready);

    int nivel = (strcmp(algoritmo, "CMN") == 0) ? (int)proceso->prioridad : 0;

    if(nivel < cant_prioridades) {
        t_queue* cola = colas_ready[nivel];
        list_add_in_index(cola->elements, 0, proceso);
    }
    pthread_mutex_unlock(&mutex_ready);
    //Sin sem_post: el proceso ya tiene su token del sem de cuando entró a ready.
}

t_pcb* quitar_de_ready_por_pid(uint32_t pid) {
    pthread_mutex_lock(&mutex_ready);
    t_pcb* resultado = NULL;
    for (int nivel = 0; nivel < cant_prioridades && resultado == NULL; nivel++) {
        t_list* elementos = colas_ready[nivel]->elements;
        for (int i = 0; i < list_size(elementos); i++) {
            t_pcb* p = list_get(elementos, i);
            if (p->pid == pid) {
                resultado = list_remove(elementos, i);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mutex_ready);
    return resultado;
}

t_pcb* obtener_siguiente_proceso() {
    
    sem_wait(&sem_procesos_en_ready); //espera a que haya al menos un proceso.

    pthread_mutex_lock(&mutex_ready);

    t_pcb* proceso = NULL;
    for (int nivel = 0; nivel < cant_prioridades && proceso == NULL; nivel++) {
        if(queue_is_empty(colas_ready[nivel])) {
            continue;
        }
        else {
            proceso = queue_pop(colas_ready[nivel]);
        }
    }
    pthread_mutex_unlock(&mutex_ready);

    return proceso;
}

// ----------------------------- BLOCK -----------------------------

void agregar_a_block(t_pcb* proceso) { 

    pthread_mutex_lock(&mutex_block);

    list_add(lista_block, proceso);

    pthread_mutex_unlock(&mutex_block);

    proceso->tiempo_susp = temporal_create(); //empieza el contador de tiempo en block.

    crear_hilo(hilo_suspension, proceso); //hilo encargado de, si el proceso está más de lo debido bloqueado, pasarlo a susp_block.
}

t_pcb* quitar_de_block(uint32_t pid) {

    pthread_mutex_lock(&mutex_block);

    t_pcb* encontrado = NULL;
    int tamanio = list_size(lista_block);

    for (int i = 0; i < tamanio; i++) {
        t_pcb* proceso = list_get(lista_block, i);
        if (proceso->pid == pid) {
            encontrado = list_remove(lista_block, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_block);

    return encontrado;
}

static void devolver_a_block_sin_alterar_timer(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_block);

    list_add(lista_block, proceso);

    pthread_mutex_unlock(&mutex_block);

    //sin temporal_create ni crear_hilo.
}

// ----------------------------- EXECUTE -----------------------------

void agregar_a_exec(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_exec);

    list_add(lista_exec, proceso);

    pthread_mutex_unlock(&mutex_exec);
}

void quitar_de_exec(uint32_t pid) {

    pthread_mutex_lock(&mutex_exec);

    t_pcb* encontrado = NULL;
    int tamanio = list_size(lista_exec);

    for (int i = 0; i < tamanio; i++) {
        t_pcb* proceso = list_get(lista_exec, i);
        if (proceso->pid == pid) {
            encontrado = list_remove(lista_exec, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_exec);

    pthread_mutex_lock(&mutex_cpu_proceso);

    if(encontrado != NULL){
        tamanio = list_size(lista_cpu_proceso);
        for (int i = 0; i < tamanio; i++) {
            t_cpu_proceso* proceso_en_cpu = list_get(lista_cpu_proceso, i);
            if (proceso_en_cpu->pid == encontrado->pid) {
                t_cpu_proceso* removido = list_remove(lista_cpu_proceso, i);
                free(removido);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mutex_cpu_proceso);
}

void pausar_en_exec(uint32_t pid) {

    pthread_mutex_lock(&mutex_exec);

    int tamanio = list_size(lista_exec);
    for (int i = 0; i < tamanio; i++) {
        t_pcb* proceso = list_get(lista_exec, i);
        if (proceso->pid == pid) {
            list_remove(lista_exec, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_exec);
}

// ----------------------------- SUSP. READY -----------------------------

void agregar_a_susp_ready(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_susp_ready);

    list_add(listas_susp_ready[proceso->prioridad], proceso); //no chequeo implanificabilidad porque ya se chequeó en agregar_a_ready.

    pthread_mutex_unlock(&mutex_susp_ready);
}

t_pcb* quitar_de_susp_ready_por_pid(uint32_t pid) {

    pthread_mutex_lock(&mutex_susp_ready);

    t_pcb* resultado = NULL;
    for (int nivel = 0; nivel < cant_prioridades && resultado == NULL; nivel++) {
        t_list* lista = listas_susp_ready[nivel];
        for (int i = 0; i < list_size(lista); i++) {
            t_pcb* p = list_get(lista, i);
            if (p->pid == pid) {
                resultado = list_remove(lista, i);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mutex_susp_ready);

    return resultado;
}

// ----------------------------- SUSP. BLOCK -----------------------------

void agregar_a_susp_block(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_susp_block);

    list_add(listas_susp_block[proceso->prioridad], proceso); //no chequeo implanificabilidad porque ya se chequeó en agregar_a_ready.

    pthread_mutex_unlock(&mutex_susp_block);
}

static t_pcb* sacar_mas_antiguo(t_list* lista) {
    if (list_is_empty(lista)) 
        return NULL;
    if (list_size(lista) == 1) 
        return list_remove(lista, 0);

    int indice = 0;
    t_pcb* candidato = list_get(lista, 0);

    for (int i = 1; i < list_size(lista); i++) {
        t_pcb* p = list_get(lista, i);

        temporal_stop(p->tiempo_susp);
        temporal_stop(candidato->tiempo_susp);

        bool p_es_mas_antiguo = temporal_gettime(p->tiempo_susp) >= temporal_gettime(candidato->tiempo_susp);

        temporal_resume(p->tiempo_susp);
        temporal_resume(candidato->tiempo_susp);

        if (p_es_mas_antiguo) {
            candidato = p;
            indice = i;
        }
    }

    return list_remove(lista, indice);
}

t_pcb* quitar_de_susp_block() {

    pthread_mutex_lock(&mutex_susp_block);

    t_pcb* proceso = NULL;
    for (int nivel = 0; nivel < cant_prioridades && proceso == NULL; nivel++) {
        proceso = sacar_mas_antiguo(listas_susp_block[nivel]);
    }
    pthread_mutex_unlock(&mutex_susp_block);

    if (proceso != NULL)
        temporal_destroy(proceso->tiempo_susp);

    return proceso;
}

t_pcb* quitar_de_susp_block_por_pid(uint32_t pid) {

    pthread_mutex_lock(&mutex_susp_block);

    t_pcb* resultado = NULL;
    for (int nivel = 0; nivel < cant_prioridades && resultado == NULL; nivel++) {
        t_list* lista = listas_susp_block[nivel];
        int tamanio = list_size(lista);
        for (int i = 0; i < tamanio; i++) {
            t_pcb* p = list_get(lista, i);
            if (p->pid == pid) {
                resultado = list_remove(lista, i);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mutex_susp_block);

    if (resultado != NULL)
        temporal_destroy(resultado->tiempo_susp);

    return resultado;
}

// ----------------------------- MISCELÁNEOS -----------------------------

void registrar_cpu_proceso(int socket_cpu_ejecutando, uint32_t pid) { //se usa independientemente del valor de "hay_desalojo_cmn".

    pthread_mutex_lock(&mutex_cpu_proceso);

    t_cpu_proceso* entry = malloc(sizeof(t_cpu_proceso));
    entry->socket_cpu = socket_cpu_ejecutando;
    entry->pid = pid;

    list_add(lista_cpu_proceso, entry);

    pthread_mutex_unlock(&mutex_cpu_proceso);
}

void* hilo_suspension(void* arg) {
    t_pcb* proceso = (t_pcb*) arg;
    usleep(suspension_timeout * 1000);

    //si devuelve NULL, el listener ya sacó al proceso de BLOCK entonces no hay que hacer nada.
    t_pcb* encontrado = quitar_de_block(proceso->pid);
    if (encontrado == NULL)
        return NULL;

    //todavía en BLOCK. Suspender.
    cambiar_estado(proceso, ESTADO_SUSP_BLOCK, logger);
    agregar_a_susp_block(proceso);

    pthread_mutex_lock(&mutex_socket_km_operaciones);
    //notifica al KM.
    enviar_opcode(socket_kernel_memory_operaciones, KM_SUSPENDER_PROCESO);
    enviar_uint32(socket_kernel_memory_operaciones, proceso->pid);
    
    op_code ack;
    if (recibir_opcode(socket_kernel_memory_operaciones, &ack) <= 0)
        log_error(logger, "## (%d) No se pudo confirmar la suspensión con Kernel Memory", proceso->pid);

    pthread_mutex_unlock(&mutex_socket_km_operaciones);

    return NULL;
}

void intentar_reanudar_proceso() { //recolecta candidatos por nivel de prioridad dentro de cada nivel: primero SUSP_READY, después SUSP_BLOCK. Trata de reanudarlos una sola vez.
    t_list* candidatos = list_create();

    pthread_mutex_lock(&mutex_susp_ready);

    pthread_mutex_lock(&mutex_susp_block);

    for (int nivel = 0; nivel < cant_prioridades; nivel++) {
        t_pcb* p;
        //SUSP_READY de este nivel (ya terminaron IO) (tiempo_susp destuido, por eso no uso sacar_mas_antiguo).
        while (!list_is_empty(listas_susp_ready[nivel])) {
            t_pcb* p = list_remove(listas_susp_ready[nivel], 0);
            list_add(candidatos, p);
        }
        //SUSP_BLOCK de este nivel (todavía esperando IO).
        while ((p = sacar_mas_antiguo(listas_susp_block[nivel])) != NULL)
            list_add(candidatos, p);
    }

    pthread_mutex_unlock(&mutex_susp_block);

    pthread_mutex_unlock(&mutex_susp_ready);

    //intento reanudar cada candidato exactamente una vez.
    int tamanio = list_size(candidatos);
    for (int i = 0; i < tamanio; i++) {
        t_pcb* proceso = list_get(candidatos, i);

        pthread_mutex_lock(&mutex_socket_km_operaciones);

        enviar_opcode(socket_kernel_memory_operaciones, KM_REANUDAR_PROCESO);
        enviar_uint32(socket_kernel_memory_operaciones, proceso->pid);

        op_code ack;
        bool km_confirmo = recibir_opcode(socket_kernel_memory_operaciones, &ack);

        pthread_mutex_unlock(&mutex_socket_km_operaciones);

        if (!km_confirmo)
            log_error(logger, "## (%d) Se perdió la conexión con Kernel Memory durante REANUDAR_PROCESO", proceso->pid);

        if (km_confirmo && ack == RESPUESTA_OK) {
            if (proceso->estado == ESTADO_SUSP_READY) {
                cambiar_estado(proceso, ESTADO_READY, logger);
                agregar_a_ready(proceso);
            } else {
                //SUSP_BLOCK todavía en IO, va a BLOCK (el listener lo va a mover a READY).
                cambiar_estado(proceso, ESTADO_BLOCK, logger);
                devolver_a_block_sin_alterar_timer(proceso);
            }
        } else {
            //no hay memoria. Lo devuelvo a su lista original.
            if (proceso->estado == ESTADO_SUSP_READY)
                agregar_a_susp_ready(proceso);
            else
                agregar_a_susp_block(proceso);
        }
    }
    list_destroy(candidatos);
}

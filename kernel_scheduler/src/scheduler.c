#include "scheduler.h"
#include "utils/hilos.h"

//listas para cada estado
t_queue** colas_ready;
t_list* lista_block;
t_list* lista_exec;
t_list** listas_susp_ready;
t_list** listas_susp_block;

//mutexes para cada lista
pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_block;
pthread_mutex_t mutex_exec;
pthread_mutex_t mutex_susp_ready;
pthread_mutex_t mutex_susp_block;

//semáforo productor-consumidor de colas_ready
sem_t sem_procesos_en_ready;

//lista de pares (cpu,pid). Solo se usa si "hay_desalojo_cmn" es true
static t_list* lista_cpu_proceso;
static pthread_mutex_t mutex_cpu_proceso;

//estructura "estática". No poner en el .h
typedef struct {
    int socket_cpu;
    uint32_t pid;
} t_cpu_proceso;


void inicializar_colas_ready(int prioridades) {
    colas_ready = malloc(sizeof(t_queue*) * cant_prioridades);
    for (int i = 0; i < cant_prioridades; i++)
        colas_ready[i] = queue_create();
}

void inicializar_listas_susp_ready(int prioridades) {
    listas_susp_ready = malloc(sizeof(t_list*) * cant_prioridades);
    for (int i = 0; i < cant_prioridades; i++)
        listas_susp_ready[i] = list_create();
}

void inicializar_listas_susp_block(int prioridades) {
    listas_susp_block = malloc(sizeof(t_list*) * cant_prioridades);
    for (int i = 0; i < cant_prioridades; i++)
        listas_susp_block[i] = list_create();
}

void inicializar_ks_planificador() {
    //inicializo colas
    inicializar_colas_ready(cant_prioridades);

    //inicializo listas
    lista_block = list_create();
    lista_exec = list_create();
    inicializar_listas_susp_ready(cant_prioridades);
    inicializar_listas_susp_block(cant_prioridades);

    //inicializo mutexes
    pthread_mutex_init(&mutex_ready, NULL);
    pthread_mutex_init(&mutex_block, NULL);
    pthread_mutex_init(&mutex_exec, NULL);
    pthread_mutex_init(&mutex_susp_ready, NULL);
    pthread_mutex_init(&mutex_susp_block, NULL);

    sem_init(&sem_procesos_en_ready, 0, 0);
}

// ------------------- READY --------------------

static int obtener_socket_cpu_de(uint32_t pid) {

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

void agregar_a_ready(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_ready);

    if(proceso->prioridad < cant_prioridades) {
        queue_push(colas_ready[proceso->prioridad], proceso);
    } else {
        log_warning(logger, "El proceso %d tiene una prioridad implanificable", proceso->prioridad);
    }

    pthread_mutex_unlock(&mutex_ready);

    sem_post(&sem_procesos_en_ready);   // Avisa que hay un proceso en colas_ready

    if(hay_desalojo_cmn) {              // Viene del main. Solo es true si el algoritmo de planificación es CMN y si el desalojo entre colas está habilitado

        pthread_mutex_lock(&mutex_exec);

        for (int i = 0; i < list_size(lista_exec); i++) {
            t_pcb* en_ejecucion = list_get(lista_exec, i);
            if (en_ejecucion->prioridad > proceso->prioridad) { //mayor priordad => menor número
                int socket_cpu_ejecutando = obtener_socket_cpu_de(en_ejecucion->pid);
                if (socket_cpu_ejecutando != -1) {
                    log_info(logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso (%d) con prioridad %d", en_ejecucion->pid, en_ejecucion->prioridad, proceso->pid, proceso->prioridad);
                    marcar_interrupcion(socket_cpu_ejecutando);  // usa el mecanismo de tick progress
                }
            break;
            }
        }
        pthread_mutex_unlock(&mutex_exec);
    }
}

t_pcb* obtener_siguiente_proceso() {
    
    sem_wait(&sem_procesos_en_ready);

    pthread_mutex_lock(&mutex_ready);

    t_pcb* proceso = NULL;
    for (int nivel = 0; nivel < cant_prioridades && proceso == NULL; nivel++)
        if(queue_size(colas_ready[nivel]) == 0) {
            continue;
        }
        else {
            proceso = queue_pop(colas_ready[nivel]);
        }


    pthread_mutex_unlock(&mutex_ready);

    return proceso;
}

// -------------- BLOCK -----------------------

void agregar_a_block(t_pcb* proceso) { 

    pthread_mutex_lock(&mutex_block);

    list_add(lista_block, proceso);

    pthread_mutex_unlock(&mutex_block);

    proceso->tiempo_susp = temporal_create();   //empieza el contador de tiempo en block

    crear_hilo(hilo_suspension, proceso);       //hilo encargado de, si el proceso está más de lo debido bloqueado, pasarlo a susp_block
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

static void devolver_a_block_sin_timer(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_block);

    list_add(lista_block, proceso);

    pthread_mutex_unlock(&mutex_block);
    // sin temporal_create ni crear_hilo
}

// ----------------- EXECUTE -----------------------

void agregar_a_exec(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_exec);

    list_add(lista_exec, proceso);

    pthread_mutex_unlock(&mutex_exec);
}

void registrar_cpu_proceso(int socket_cpu_ejecutando, uint32_t pid) {

    pthread_mutex_lock(&mutex_cpu_proceso);

    t_cpu_proceso* entry = malloc(sizeof(t_cpu_proceso));
    entry->socket_cpu = socket_cpu_ejecutando;
    entry->pid = pid;
    list_add(lista_cpu_proceso, entry);

    pthread_mutex_unlock(&mutex_cpu_proceso);
}

void quitar_de_exec(uint32_t pid) {

    pthread_mutex_lock(&mutex_exec);

    t_pcb* proceso = NULL;
    for (int i = 0; i < list_size(lista_exec); i++) {
        proceso = list_get(lista_exec, i);
        if (proceso->pid == pid) {
            list_remove(lista_exec, i);
            break;
        }
        else {
            proceso = NULL;
        }
    }
    pthread_mutex_unlock(&mutex_exec);

    pthread_mutex_lock(&mutex_cpu_proceso);

    if(proceso != NULL){
        for (int i = 0; i < list_size(lista_cpu_proceso); i++) {
            t_cpu_proceso* proceso_en_cpu = list_get(lista_cpu_proceso, i);
            if (proceso_en_cpu->pid == proceso->pid) {
                list_remove(lista_cpu_proceso, i);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mutex_cpu_proceso);

}

// ----------------- SUSP. READY -----------------------

void agregar_a_susp_ready(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_susp_ready);

    list_add(listas_susp_ready[proceso->prioridad], proceso);

    pthread_mutex_unlock(&mutex_susp_ready);
}

static t_pcb* quitar_primero_de_susp_ready_nivel(int nivel) {
    return sacar_mas_antiguo(listas_susp_ready[nivel]);
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

// ----------------- SUSP. BLOCK -----------------------

void agregar_a_susp_block(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_susp_block);

    list_add(listas_susp_block[proceso->prioridad], proceso);

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
    for (int nivel = 0; nivel < cant_prioridades && proceso == NULL; nivel++)
        proceso = sacar_mas_antiguo(listas_susp_block[nivel]);

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
        for (int i = 0; i < list_size(lista); i++) {
            t_pcb* p = list_get(lista, i);
            if (p->pid == pid) {
                resultado = list_remove(lista, i);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mutex_susp_block);

    if (proceso != NULL)
        temporal_destroy(proceso->tiempo_susp);

    return resultado;
}

void* hilo_suspension(void* arg) {
    t_pcb* proceso = (t_pcb*) arg;
    usleep(suspension_timeout * 1000);

    if (proceso->estado != ESTADO_BLOCK)
        return NULL;
        // El listener ya lo movió a READY — no hacer nada

    // Todavía en BLOCK — suspender
    quitar_de_block(proceso->pid);
    cambiar_estado(proceso, ESTADO_SUSP_BLOCK, logger);
    agregar_a_susp_block(proceso);

    // Notificar a KM
    pthread_mutex_lock(&mutex_socket_km);

    enviar_opcode(socket_kernel_memory_operaciones, KM_SUSPENDER_PROCESO);
    enviar_uint32(socket_kernel_memory_operaciones, proceso->pid);
    
    op_code ack;
    recibir_opcode(socket_kernel_memory_operaciones, &ack);

    pthread_mutex_unlock(&mutex_socket_km);

    return NULL;
}

void intentar_reanudar_proceso() {

    // Recolectar candidatos por nivel de prioridad dentro de cada nivel: primero SUSP_READY, después SUSP_BLOCK
    t_list* candidatos = list_create();

    pthread_mutex_lock(&mutex_susp_ready);

    pthread_mutex_lock(&mutex_susp_block);

    for (int nivel = 0; nivel < cant_prioridades; nivel++) {
        t_pcb* p;
        // SUSP_READY de este nivel — ya terminaron IO
        while ((p = sacar_mas_antiguo(listas_susp_ready[nivel])) != NULL)
            list_add(candidatos, p);
        // SUSP_BLOCK de este nivel — todavía esperando IO
        while ((p = sacar_mas_antiguo(listas_susp_block[nivel])) != NULL)
            list_add(candidatos, p);
    }

    pthread_mutex_unlock(&mutex_susp_block);

    pthread_mutex_unlock(&mutex_susp_ready);

    // Intentar reanudar cada candidato exactamente una vez
    for (int i = 0; i < list_size(candidatos); i++) {
        t_pcb* proceso = list_get(candidatos, i);

        pthread_mutex_lock(&mutex_socket_km);

        enviar_opcode(socket_kernel_memory_operaciones, KM_REANUDAR_PROCESO);
        enviar_uint32(socket_kernel_memory_operaciones, proceso->pid);

        op_code ack;
        recibir_opcode(socket_kernel_memory_operaciones, &ack);

        pthread_mutex_unlock(&mutex_socket_km);

        if (ack == RESPUESTA_OK) {
            if (proceso->estado == ESTADO_SUSP_READY) {
                cambiar_estado(proceso, ESTADO_READY, logger);
                agregar_a_ready(proceso);
            } else {
                // SUSP_BLOCK — todavía en IO, va a BLOCK (el listener lo moverá a READY)
                cambiar_estado(proceso, ESTADO_BLOCK, logger);
                devolver_a_block_sin_timer(proceso);
            }
        } else {
            // No hay memoria — volver a su lista original
            if (proceso->estado == ESTADO_SUSP_READY)
                agregar_a_susp_ready(proceso);
            else
                agregar_a_susp_block(proceso);
        }
    }
    list_destroy(candidatos);
}

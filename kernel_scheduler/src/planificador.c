#include "procesos.h"
#include "scheduler.h"
#include "IO_manager.h"
#include "utils/hilos.h"
#include "planificador.h"
#include "mutex_manager.h"
#include <string.h>
#include <unistd.h>

//cola de cpus libres.
static t_queue* cola_cpus_libres;
static pthread_mutex_t mutex_cpus;

//semáforo productor-consumidor de cpus libres.
static sem_t sem_cpus_libres;

//lista de timers de las cpus ocupadas.
static t_list* lista_timers;
static pthread_mutex_t mutex_timers;

//lista de cpus con una interrupción por cumplir.
static t_list* cpus_con_interrupcion;
static pthread_mutex_t mutex_interrupciones;

void inicializar_ks_cpu_manager() {
    //inicializo colas.
    cola_cpus_libres = queue_create();

    //inicializo listas.
    lista_timers = list_create();
    cpus_con_interrupcion = list_create();

    //inicializo mutexes.
    pthread_mutex_init(&mutex_cpus, NULL);
    pthread_mutex_init(&mutex_timers, NULL);
    pthread_mutex_init(&mutex_interrupciones, NULL);

    //inicializo semáforo productor-consumidor.
    sem_init(&sem_cpus_libres, 0, 0);
}

void agregar_cpu_libre(int socket_cpu) {

    pthread_mutex_lock(&mutex_cpus);

    int* socket = malloc(sizeof(int));
    *socket = socket_cpu;
    queue_push(cola_cpus_libres, socket);

    pthread_mutex_unlock(&mutex_cpus);

    sem_post(&sem_cpus_libres); //avisa que hay una cpu en cola_cpus_libres.
}

static int obtener_cpu_libre() {

    sem_wait(&sem_cpus_libres);

    pthread_mutex_lock(&mutex_cpus);

    int* socket = queue_pop(cola_cpus_libres);

    pthread_mutex_unlock(&mutex_cpus);

    int fd = *socket; //File Descriptor (fd) = socket_id
    free(socket);

    return fd;
}

// ----------------------------- DISPATCHER -----------------------------

static void guardar_timer(int socket_cpu, pthread_t timer) {

    pthread_mutex_lock(&mutex_timers);

    //creo y hago uso de una estructura cpu timer.
    t_cpu_timer* entry = malloc(sizeof(t_cpu_timer));
    entry->socket_cpu = socket_cpu;
    entry->hilo_timer = timer;
    //agrego la estructura a la lista de timers.
    list_add(lista_timers, entry);

    pthread_mutex_unlock(&mutex_timers);
}

static void* hilo_quantum(void* arg) {
    t_args_timer* args = (t_args_timer*) arg;
    int socket_cpu = args->socket_cpu;
    uint32_t quantum = args->quantum_ms;
    free(args);

    //duermo lo especificado por el quantum.
    usleep(quantum * 1000);

    // Punto de cancelación extra: si pthread_cancel llegó justo después del
    // usleep pero antes de marcar_interrupcion, acá se captura y el hilo
    // termina sin marcar la interrupción, que ya no corresponde.
    pthread_testcancel();

    //si llegamos a acá no se mató al hilo y venció el quantum. Mandamos interrupción a la CPU.
    log_info(logger, "Quantum vencido, enviando interrupción a CPU %d", socket_cpu);
    marcar_interrupcion(socket_cpu);

    return NULL;
}

void* hilo_dispatcher(void* arg) {
    //creo un array de quantums con tamaño dinámico.
    uint32_t* quantum = malloc(sizeof(uint32_t) * cant_prioridades);

    //me fijo si el algoritmo es CMN.
    if(strcmp(algoritmo, "CMN") == 0) {
        for(int prioridad = 0; prioridad < cant_prioridades; prioridad++) {
            if(queues_algoritmos[prioridad] != NULL) {
                //me fijo si el algoritmo de planificación de una prioridad dada es RR.
                if(strcmp(queues_algoritmos[prioridad], "RR") == 0) {
                    //guardo en una posición congruente a la prioridad dentro del array de quantums el valor del quantum.
                    quantum[prioridad] = config_get_int_value(config, "RR_QUANTUM");
                } else {
                    quantum[prioridad] = 0;
                }
            }
        }
    }
    //me fijo si el algoritmo es RR.
    if(strcmp(algoritmo, "RR") == 0)
        //guardo el quantum.
        quantum[0] = config_get_int_value(config, "RR_QUANTUM");

    while (1) {
        
        //obtengo el siguiente proceso a ejecutar y una cpu libre.
        t_pcb* proceso = obtener_siguiente_proceso();
        int cpu = obtener_cpu_libre();

        //compruebo que no estemos compactando en este momento.
        sem_wait(&sem_compactacion);
        sem_post(&sem_compactacion);

        //le cambio el estado al proceso y lo agrego a la lista adecuada.
        cambiar_estado(proceso, ESTADO_EXEC, logger);
        agregar_a_exec(proceso);

        //le envio a la cpu el pid del proceso y registro el par (cpu,pid).
        enviar_uint32(cpu, proceso->pid);
        registrar_cpu_proceso(cpu, proceso->pid);

        //me fijo si el algoritmo es CMN.
        if(strcmp(algoritmo, "CMN") == 0) {    
            //me fijo si el algoritmo de planificación de la prioridad del proceso es RR.
            if (strcmp(queues_algoritmos[proceso->prioridad], "RR") == 0) {
                //creo una variable args timer y le paso la cpu en uso y el quantum de la prioridad.
                t_args_timer* args = malloc(sizeof(t_args_timer));
                args->socket_cpu = cpu;
                args->quantum_ms = quantum[proceso->prioridad];

                //creo un hilo manualmente para interrumpir la cpu si el proceso se pasa de tiempo.
                pthread_t timer;
                pthread_create(&timer, NULL, hilo_quantum, args);
                pthread_detach(timer);

                //guardo el timer para cancelarlo. Por eso lo creé manualmente.
                guardar_timer(cpu, timer);
            }
        } else {
            //me fijo si el algoritmo es RR.
            if (strcmp(algoritmo, "RR") == 0) {
                //creo una variable args timer y le paso la cpu en uso y el quantum de la prioridad.
                t_args_timer* args = malloc(sizeof(t_args_timer));
                args->socket_cpu = cpu;
                args->quantum_ms = quantum[0];

                //creo un hilo manualmente para interrumpir la cpu si el proceso se pasa de tiempo.
                pthread_t timer;
                pthread_create(&timer, NULL, hilo_quantum, args);
                pthread_detach(timer);
                
                //guardo el timer para cancelarlo. Por eso lo creé manualmente.
                guardar_timer(cpu, timer);
            }
        }
    }
    free(quantum);
    
    return NULL;
}

// ----------------------------- MANEJADOR DE SYSCALLS -----------------------------

static bool consumir_interrupcion(int socket_cpu) {

    pthread_mutex_lock(&mutex_interrupciones);

    int tamanio = list_size(cpus_con_interrupcion);
    for (int i = 0; i < tamanio; i++) {
        int* s = list_get(cpus_con_interrupcion, i);
        if (*s == socket_cpu) {
            list_remove(cpus_con_interrupcion, i);
            free(s);

            pthread_mutex_unlock(&mutex_interrupciones);

            return true;
        }
    }
    pthread_mutex_unlock(&mutex_interrupciones);

    return false;
}

void manejar_tick_progress(int socket_cpu, t_pcb* proceso) {

    //me fijo si hay alguna interrupción por acatar.
    if (consumir_interrupcion(socket_cpu)) {
        //Le aviso a la CPU que pare.
        enviar_uint32(socket_cpu, HAY_INTERRUPCION);

        //cancelo su timer y muevo a ready al proceso que estaba ejecutando.
        cancelar_timer(socket_cpu);
        quitar_de_exec(proceso->pid);
        cambiar_estado(proceso, ESTADO_READY, logger);

        pthread_mutex_lock(&mutex_desalojo_compactacion);

        if(desalojo_por_compactacion) {
            agregar_al_principio_de_ready(proceso);
            log_info(logger, "## (%d) - Desalojado por compactación", proceso->pid);

            //aviso al hilo de escuchar_kernel_memory que esta CPU ya se desalojó.
            sem_post(&sem_desalojo_ok);
        }
        else {
            agregar_a_ready(proceso);
            log_info(logger, "## (%d) - Desalojado por fin de quantum", proceso->pid);
        }
        pthread_mutex_unlock(&mutex_desalojo_compactacion);

        //libero la cpu.
        agregar_cpu_libre(socket_cpu);

    } else {
        //no hay interrupción. Aviso a la cpu que siga.
        enviar_uint32(socket_cpu, SIN_INTERRUPCION);
    }
}

void manejar_syscall_io_cpu(int socket_cpu, t_pcb* proceso) {
    //creo y hago uso de las variables necesarias para recibir los parámetros de la syscall.
    uint32_t tipo_inst;
    char param1[32] = {0};
    char param2[32] = {0};
    recibir_uint32(socket_cpu, &tipo_inst);
    recibir_string(socket_cpu, param1, sizeof(param1));
    recibir_string(socket_cpu, param2, sizeof(param2));

    //creo una estructura io request y le asigno el pid del proceso que busca ser atentido.
    t_io_request req = {0};
    req.pid = proceso->pid;

    //relleno los campos restantes de la estructura io request dependiendo el tipo de instancia IO a usar.
    switch ((tipo_instruccion)tipo_inst) {
        case INST_SLEEP:
            req.tipo = TIPO_IO_SLEEP;
            req.sleep_ms = (uint32_t)atoi(param1);   //uso param1 como tiempo en milisegundos que debe dormir el proceso.
            break;

        case INST_STDIN:
            req.tipo = TIPO_IO_STDIN;
            req.dir_fisica = (uint32_t)atoi(param1); //uso param1 como dir donde guardar lo leído.
            req.size = (uint32_t)atoi(param2);       //uso param2 como tamaño de lo que debemos leer.
            break;

        case INST_STDOUT:
            req.tipo = TIPO_IO_STDOUT;
            req.dir_fisica = (uint32_t)atoi(param1); //uso param1 como dir donde guardar lo leído.
            req.size = (uint32_t)atoi(param2);       //uso param2 como tamaño de lo que debemos leer.
            break;

        default:
            log_error(logger, "## (%d) - Tipo de syscall desconocido: %u", proceso->pid, tipo_inst);
            break;
    }
    cancelar_timer(socket_cpu);
    manejar_syscall_io(proceso, &req, socket_cpu, logger);
}

void manejar_syscall_mutex_create(int socket_cpu, t_pcb* proceso) {
    //creo y hago uso de las variables necesarias para recibir los parámetros de la syscall.
    uint32_t tipo_inst;
    char nombre[32] = {0};
    char param2[32] = {0};
    recibir_uint32(socket_cpu, &tipo_inst);
    recibir_string(socket_cpu, nombre, sizeof(nombre));
    //si bien param2 no se usa hay que llevar a cabo la acción de "recibir nada" para no desincronizar.
    recibir_string(socket_cpu, param2, sizeof(param2));

    log_info(logger, "## (%d) - Solicitó syscall: MUTEX_CREATE", proceso->pid);
    //cancelo el timer de la cpu que alojaba al proceso.
    cancelar_timer(socket_cpu);

    crear_mutex(nombre);
    //recreo el timer de la cpu que va a volver a alojar al proceso.
    recrear_timer(socket_cpu, proceso);
    //una vez termino, el proceso vuelve a la cpu en la que estaba previamente.
    enviar_uint32(socket_cpu, proceso->pid);
}

void manejar_syscall_mem_alloc(int socket_cpu, t_pcb* proceso) {
    //creo y hago uso de las variables necesarias para recibir los parámetros de la syscall.
    uint32_t tipo_inst;
    char param1[32] = {0};
    char param2[32] = {0};
    recibir_uint32(socket_cpu, &tipo_inst);
    recibir_string(socket_cpu, param1, sizeof(param1));
    recibir_string(socket_cpu, param2, sizeof(param2));

    log_info(logger, "## (%d) - Solicitó syscall: MEM_ALLOC", proceso->pid);

    //obtengo el id del segmento y su tamaño.
    uint32_t id_segmento = (uint32_t)atoi(param1);
    uint32_t tamanio = (uint32_t)atoi(param2);

    //cancelo el timer de la cpu que alojaba al proceso.
    cancelar_timer(socket_cpu);

    pausar_en_exec(proceso->pid);

    pthread_mutex_lock(&mutex_socket_km_operaciones);

    //notifico al KM para que haga la allocation junto con los parámetros necesarios.
    enviar_opcode(socket_kernel_memory_operaciones, KM_MEM_ALLOC);
    enviar_uint32(socket_kernel_memory_operaciones, proceso->pid);
    enviar_uint32(socket_kernel_memory_operaciones, id_segmento);
    enviar_uint32(socket_kernel_memory_operaciones, tamanio);

    op_code ack;
    recibir_opcode(socket_kernel_memory_operaciones, &ack);

    pthread_mutex_unlock(&mutex_socket_km_operaciones);

    if (ack != RESPUESTA_OK)
        log_error(logger, "## (%d) MEM_ALLOC falló", proceso->pid);

    agregar_a_exec(proceso);

    //recreo el timer de la cpu que va a volver a alojar al proceso.
    recrear_timer(socket_cpu, proceso);
    //una vez termino, el proceso vuelve a la cpu en la que estaba previamente.
    enviar_uint32(socket_cpu, proceso->pid);
}

void manejar_syscall_mem_free(int socket_cpu, t_pcb* proceso) {
    //creo y hago uso de las variables necesarias para recibir los parámetros de la syscall.
    uint32_t tipo_inst;
    char param1[32] = {0};
    char param2[32] = {0};
    recibir_uint32(socket_cpu, &tipo_inst);
    recibir_string(socket_cpu, param1, sizeof(param1));
    //si bien param2 no se usa hay que llevar a cabo la acción de "recibir nada" para no desincronizar.
    recibir_string(socket_cpu, param2, sizeof(param2));

    log_info(logger, "## (%d) - Solicitó syscall: MEM_FREE", proceso->pid);

    //obtengo el id del segmento a liberar.
    uint32_t id_segmento = (uint32_t)atoi(param1);

    //cancelo el timer de la cpu que alojaba al proceso.
    cancelar_timer(socket_cpu);

    pausar_en_exec(proceso->pid);

    pthread_mutex_lock(&mutex_socket_km_operaciones);

    enviar_opcode(socket_kernel_memory_operaciones, KM_MEM_FREE);
    enviar_uint32(socket_kernel_memory_operaciones, proceso->pid);
    enviar_uint32(socket_kernel_memory_operaciones, id_segmento);

    op_code ack;
    recibir_opcode(socket_kernel_memory_operaciones, &ack);

    pthread_mutex_unlock(&mutex_socket_km_operaciones);

    if (ack != RESPUESTA_OK)
        log_error(logger, "## (%d) MEM_FREE falló para segmento %u", proceso->pid, id_segmento);

    agregar_a_exec(proceso);

    //recreo el timer de la cpu que va a volver a alojar al proceso.
    recrear_timer(socket_cpu, proceso);
    //una vez termino, el proceso vuelve a la cpu en la que estaba previamente.
    enviar_uint32(socket_cpu, proceso->pid);
}

void manejar_syscall_exit(int socket_cpu, t_pcb* proceso) {
    //creo y hago uso de las variables necesarias para recibir los parámetros de la syscall.
    uint32_t tipo_inst;
    char param1[32] = {0};
    char param2[32] = {0};
    recibir_uint32(socket_cpu, &tipo_inst);
    recibir_string(socket_cpu, param1, sizeof(param1));
    recibir_string(socket_cpu, param2, sizeof(param2));

    log_info(logger, "## (%d) - Solicitó syscall: EXIT", proceso->pid);
    
    //cancelo el timer de la cpu porque nunca podría terminar de usarse un proceso muerto.
    cancelar_timer(socket_cpu);
    quitar_de_exec(proceso -> pid);

    if (strcmp(param1, "SEG_FAULT") == 0)
        log_error(logger, "## (%d) finalizó su ejecución con motivo de %s", proceso->pid, param1);
    else
        log_info(logger, "## (%d) finalizó su ejecución con motivo de EXIT", proceso->pid);
    
    cambiar_estado(proceso, ESTADO_EXIT, logger);

    //notifico al KM que libere la memoria del proceso.
    pthread_mutex_lock(&mutex_socket_km_operaciones);

    enviar_opcode(socket_kernel_memory_operaciones, KM_FINALIZAR_PROCESO);
    enviar_uint32(socket_kernel_memory_operaciones, proceso->pid);

    op_code ack;
    recibir_opcode(socket_kernel_memory_operaciones, &ack);

    pthread_mutex_unlock(&mutex_socket_km_operaciones);

    remover_de_activos_global(proceso->pid);

    //destruyo el proceso, libero la cpu que lo manejaba.
    destruir_pcb(proceso);
    agregar_cpu_libre(socket_cpu);

    //trato de reanudar procesos en estado SUSP. READY o SUSP. BLOCK.
    intentar_reanudar_proceso();
}

void manejar_iniciar_proceso(int socket_cpu, t_pcb* llamador) {
    //creo y hago uso de las variables necesarias para recibir los parámetros (es una syscall pero tiene un protocolo distinto).
    char path[BUFFER_SIZE] = {0};
    uint32_t prioridad;
    recibir_string(socket_cpu, path, sizeof(path));
    recibir_uint32(socket_cpu, &prioridad);

    log_info(logger, "## (%d) - Solicitó syscall: INIT_PROC", llamador->pid);

    crear_proceso(path, prioridad);

    //muevo el proceso llamador de vuelta a ready y libero la CPU.
    cancelar_timer(socket_cpu);
    quitar_de_exec(llamador->pid);
    cambiar_estado(llamador, ESTADO_READY, logger);
    agregar_a_ready(llamador);
    agregar_cpu_libre(socket_cpu);
}

// ----------------------------- FUNCIONES AUXILIARES -----------------------------

void crear_proceso(const char* path, uint32_t prioridad) {

    pthread_mutex_lock(&mutex_pid);

    //obtengo el pid del nuevo proceso y luego incremento la variable global.
    uint32_t pid_nuevo = proximo_pid++;

    pthread_mutex_unlock(&mutex_pid);

    //creo su pcb y lo agrego a la lista global de procesos activos.
    t_pcb* nuevo = crear_pcb(pid_nuevo, prioridad);

    pthread_mutex_lock(&mutex_p_activos);

    list_add(p_activos_global, nuevo);

    pthread_mutex_unlock(&mutex_p_activos);

    log_info(logger, "## (%d) Se crea el proceso - Estado: NEW", pid_nuevo);

    pthread_mutex_lock(&mutex_socket_km_operaciones);

    //envio al Kernel Memory la instrucción de crear el contexto del proceso y de leer sus instrucciones.
    enviar_opcode(socket_kernel_memory_operaciones, KM_CREAR_PROCESO);
    enviar_uint32(socket_kernel_memory_operaciones, pid_nuevo);
    enviar_string(socket_kernel_memory_operaciones, path);

    op_code ack;
    bool creacion_ok = (recibir_opcode(socket_kernel_memory_operaciones, &ack) > 0 && ack == RESPUESTA_OK);
    
    pthread_mutex_unlock(&mutex_socket_km_operaciones);

    if (!creacion_ok) {
        log_error(logger, "Kernel Memory rechazó la creación del proceso PID %d", pid_nuevo);

        pthread_mutex_lock(&mutex_p_activos);

        //limpio el PCB huérfano de la lista global.
        for (int i = 0; i < list_size(p_activos_global); i++) {
            t_pcb* p = list_get(p_activos_global, i);
            if (p->pid == pid_nuevo) {
                list_remove(p_activos_global, i);
                destruir_pcb(p);
                break;
            }
        }
        pthread_mutex_unlock(&mutex_p_activos);

        return;
    }
    //una vez hecho todo cambio el estado del proceso a ready y lo agrego a la cola que le corresponde.
    cambiar_estado(nuevo, ESTADO_READY, logger);
    agregar_a_ready(nuevo);
}

void recrear_timer(int socket_cpu, t_pcb* proceso) {
    if (strcmp(algoritmo, "RR") == 0) {
        t_args_timer* args = malloc(sizeof(t_args_timer));
        args->socket_cpu   = socket_cpu;
        args->quantum_ms   = config_get_int_value(config, "RR_QUANTUM");

        pthread_t timer;
        pthread_create(&timer, NULL, hilo_quantum, args);
        pthread_detach(timer);
        guardar_timer(socket_cpu, timer);

    } else if (strcmp(algoritmo, "CMN") == 0) {
        if (strcmp(queues_algoritmos[proceso->prioridad], "RR") == 0) {
            t_args_timer* args = malloc(sizeof(t_args_timer));
            args->socket_cpu   = socket_cpu;
            args->quantum_ms   = config_get_int_value(config, "RR_QUANTUM");

            pthread_t timer;
            pthread_create(&timer, NULL, hilo_quantum, args);
            pthread_detach(timer);
            guardar_timer(socket_cpu, timer);
        }
    }
}

void cancelar_timer(int socket_cpu) {

    pthread_mutex_lock(&mutex_timers);
    
    int tamanio = list_size(lista_timers);
    for (int i = 0; i < tamanio; i++) {
        t_cpu_timer* entry = list_get(lista_timers, i);
        if (entry->socket_cpu == socket_cpu) {
            //mato al hilo que al vencerse el timer hubiese disparado una interrupción.
            pthread_cancel(entry->hilo_timer);
            list_remove(lista_timers, i);
            free(entry);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_timers);
}

void marcar_interrupcion(int socket_cpu) {

    pthread_mutex_lock(&mutex_interrupciones);

    int* s = malloc(sizeof(int));
    *s = socket_cpu;
    //agregamos la cpu a la lista de cpus con una interrupción por cumplir.
    list_add(cpus_con_interrupcion, s);

    pthread_mutex_unlock(&mutex_interrupciones);
}

void olvidar_interrupcion(int socket_cpu) {

    pthread_mutex_lock(&mutex_interrupciones);

    int tamanio = list_size(cpus_con_interrupcion);
    for (int i = 0; i < tamanio; i++) {
        int* s = list_get(cpus_con_interrupcion, i);
        if (*s == socket_cpu) {
            list_remove(cpus_con_interrupcion, i);
            free(s);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_interrupciones);
}

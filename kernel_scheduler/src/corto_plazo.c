#include "procesos.h"
#include "scheduler.h"
#include "IO_manager.h"
#include "corto_plazo.h"
#include <commons/config.h>
#include <string.h>
#include <unistd.h>

extern t_log* logger;
extern t_config* config;

//-- COLA DE CPUs LIBRES ----------------------

static t_queue* cola_cpus_libres;
static pthread_mutex_t mutex_cpus;
static sem_t sem_cpus_libres;
static t_list* lista_timers;
static pthread_mutex_t mutex_timers;

void inicializar_corto_plazo() {
    cola_cpus_libres = queue_create();
    pthread_mutex_init(&mutex_cpus, NULL);
    sem_init(&sem_cpus_libres, 0, 0);

    lista_timers = list_create();
    pthread_mutex_init(&mutex_timers, NULL);
}

void agregar_cpu_libre(int socket_cpu) {
    pthread_mutex_lock(&mutex_cpus);
    int* socket = malloc(sizeof(int));
    *socket = socket_cpu;
    queue_push(cola_cpus_libres, socket);
    pthread_mutex_unlock(&mutex_cpus);
    sem_post(&sem_cpus_libres);
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

//-- DISPATCHER -----------------------------

void* hilo_dispatcher(void* arg) {
    uint32_t quantum = 0;
    char* algoritmo = config_get_string_value(config, "PLANIFICATION_ALGORITHM");

    if(strcmp(algoritmo, "RR")==0) {
        quantum = config_get_int_value(config, "RR_QUANTUM");
    }

    while (1) {
        
        t_pcb* proceso = obtener_siguiente_proceso();
        int cpu = obtener_cpu_libre();

        cambiar_estado(proceso, ESTADO_EXEC, logger);
        agregar_a_exec(proceso);

        enviar_uint32(cpu, proceso->pid);

        if (strcmp(algoritmo, "RR") == 0) {
            t_args_timer* args = malloc(sizeof(t_args_timer));
            args->socket_cpu = cpu;
            args->quantum_ms = quantum;

            pthread_t timer;
            pthread_create(&timer, NULL, hilo_quantum, args);
            pthread_detach(timer);
            guardar_timer(cpu, timer);

            // Guardamos el timer para poder cancelarlo
            guardar_timer(cpu, timer);
        }
    }
    return NULL;
}

//-- MANEJADOR DE SYSCALLS -----------------------------

void manejar_syscall_io_cpu(int socket_cpu, t_pcb* proceso, op_code tipo_io) {

    uint32_t tipo_inst;
    char param1[32] = {0};
    char param2[32] = {0};
    recibir_uint32(socket_cpu, &tipo_inst);
    recibir_string(socket_cpu, param1, sizeof(param1));
    recibir_string(socket_cpu, param2, sizeof(param2));

    t_io_request req = {0};
    req.pid = proceso->pid;

    switch ((tipo_instruccion)tipo_inst) {
        case INST_SLEEP:
            req.tipo = TIPO_IO_SLEEP;
            req.sleep_ms = (uint32_t)atoi(param1);
            break;

        case INST_STDIN:
            req.tipo = TIPO_IO_STDIN;
            req.dir_logica = (uint32_t)atoi(param1);
            req.size = (uint32_t)atoi(param2);
            break;

        case INST_STDOUT:
            req.tipo = TIPO_IO_STDOUT;
            req.dir_logica = (uint32_t)atoi(param1);
            req.size = (uint32_t)atoi(param2);
            break;

        default:
            // No es IO: MUTEX_*, MEM_ALLOC, MEM_FREE
            log_info(logger, "## (%d) - Solicitó syscall: %u", proceso->pid, instruccion_to_string((tipo_instruccion)tipo_inst);
            cancelar_timer(socket_cpu);
            quitar_de_exec(proceso->pid);
            agregar_cpu_libre(socket_cpu);
            return;
    }

    cancelar_timer(socket_cpu);
    manejar_syscall_io(proceso, &req, logger);
    agregar_cpu_libre(socket_cpu);
}

void manejar_exit(int socket_cpu, t_pcb* proceso) {
    
    cancelar_timer(socket_cpu);
    quitar_de_exec(proceso -> pid);
    cambiar_estado(proceso, ESTADO_EXIT, logger);

    log_info(logger, "## (%d) finalizó su ejecución con motivo de EXIT", proceso -> pid);

    destruir_pcb(proceso);

    agregar_cpu_libre(socket_cpu);

}

void manejar_iniciar_proceso(int socket_cpu, int socket_kernel_memory) {
    uint32_t pid_nuevo;
    uint32_t prioridad;
    char path[256];
    recibir_uint32(socket_cpu, &pid_nuevo);
    recibir_uint32(socket_cpu, &prioridad);
    recibir_string(socket_cpu, path, sizeof(path));

    t_pcb* nuevo = crear_pcb(pid_nuevo, prioridad);
    list_add(p_activos_global, nuevo);
    log_info(logger, "## (%d) Se crea el proceso - Estado: NEW", pid_nuevo);

    enviar_opcode(socket_kernel_memory, KM_CREAR_PROCESO);
    enviar_uint32(socket_kernel_memory, pid_nuevo);
    enviar_string(socket_kernel_memory, path);

    op_code ack;
    if(recibir_opcode(socket_kernel_memory, &ack)<=0) {
        log_error(logger, "Error al recibir ACK de Kernel Memory para PID %d", pid_nuevo);
        break;
    }

    if (ack != RESPUESTA_OK) {
    log_error(logger, "Kernel Memory rechazó la creación del proceso PID %d", pid_nuevo);
        break;
    }

    cambiar_estado(nuevo, ESTADO_READY, logger);
    agregar_a_ready(nuevo);
}

void manejar_fin_quantum(int socket_cpu, t_pcb* proceso) {
    
    cancelar_timer(socket_cpu);
    quitar_de_exec(proceso -> pid);
    cambiar_estado(proceso, ESTADO_READY, logger);
    agregar_a_ready(proceso);

    log_info(logger, "## (%d) - Desalojado por fin de quantum", proceso->pid);

    agregar_cpu_libre(socket_cpu);

}

static void guardar_timer(int socket_cpu, pthread_t timer) {
    pthread_mutex_lock(&mutex_timers);
    t_cpu_timer* entry = malloc(sizeof(t_cpu_timer));
    entry -> socket_cpu  = socket_cpu;
    entry -> hilo_timer  = timer;
    list_add(lista_timers, entry);
    pthread_mutex_unlock(&mutex_timers);
}

static void cancelar_timer(int socket_cpu) {
    pthread_mutex_lock(&mutex_timers);
    for (int i = 0; i < list_size(lista_timers); i++) {
        t_cpu_timer* entry = list_get(lista_timers, i);
        if (entry->socket_cpu == socket_cpu) {
            pthread_cancel(entry->hilo_timer);
            list_remove(lista_timers, i);
            free(entry);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_timers);
}

static void* hilo_quantum(void* arg) {
    t_args_timer* args = (t_args_timer*) arg;
    int socket_cpu  = args->socket_cpu;
    uint32_t quantum = args->quantum_ms;
    free(args);

    usleep(quantum * 1000);

    // Si llegamos acá, venció el quantum — mandamos interrupción a la CPU
    log_info(logger, "Quantum vencido, enviando interrupción a CPU %d", socket_cpu);
    enviar_opcode(socket_cpu, KS_FIN_QUANTUM);

    return NULL;
}
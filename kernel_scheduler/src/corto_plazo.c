#include "corto_plazo.h"
#include "scheduler.h"
#include "pcb.h"
#include "procesos.h"
#include "utils/conexion.h"
#include "utils/mensajes.h"
#include <stdlib.h>
#include <commons/log.h>

extern t_log* logger;

//-- COLA DE CPUs LIBRES ----------------------

static t_queue* cola_cpus_libres;
static pthread_mutex_t mutex_cpus;
static sem_t sem_cpus_libres;

void inicializar_corto_plazo() {
    cola_cpus_libres = queue_create();
    pthread_mutex_init(&mutex_cpus, NULL);
    sem_init(&sem_cpus_libres, 0, 0);
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
    while (1) {
        
        t_pcb* proceso = obtener_siguiente_proceso();
        int cpu = obtener_cpu_libre();

        cambiar_estado(proceso, ESTADO_EXEC, logger);
        agregar_a_exec(proceso);

        enviar_uint32(cpu, proceso->pid);

        log_info(logger, "## (%d) Pasa del estado READY al estado EXEC", proceso->pid);
    }
    return NULL;
}

//-- MANEJADOR DE SYSCALLS -----------------------------

void manejar_syscall_io(int socket_cpu, t_pcb* proceso, op_code tipo_io) {

    quitar_de_exec(proceso->pid);
    cambiar_estado(proceso, ESTADO_BLOCK, logger);
    agregar_a_block(proceso);

    agregar_cpu_libre(socket_cpu);

    // Reenviamos la syscall al módulo IO correspondiente
    // (el socket del IO lo tiene que proveer el contexto de conexiones)
    log_info(logger, "## (%d) - Solicitó syscall: %s", proceso->pid, tipo_io == IO_EJECUTAR ? "IO" : "DESCONOCIDA");

}

void manejar_exit(int socket_cpu, t_pcb* proceso) {
    
    quitar_de_exec(proceso -> pid);
    cambiar_estado(proceso, ESTADO_EXIT, logger);

    log_info(logger, "## (%d) finalizó su ejecución con motivo de EXIT", proceso -> pid);

    destruir_pcb(proceso);

    agregar_cpu_libre(socket_cpu);

}

void manejar_fin_quantum(int socket_cpu, t_pcb* proceso) {
    
    quitar_de_exec(proceso -> pid);
    cambiar_estado(proceso, ESTADO_READY, logger);
    agregar_a_ready(proceso);

    log_info(logger, "## (%d) - Desalojado por fin de quantum", proceso->pid);

    agregar_cpu_libre(socket_cpu);

}
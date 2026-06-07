#ifndef CORTO_PLAZO_H
#define CORTO_PLAZO_H

#include "pcb.h"
#include "utils/mensajes.h"
#include <commons/collections/queue.h>
#include <semaphore.h>
#include <pthread.h>

// Para socket_cpu -> hilo timer activo
typedef struct {
    int socket_cpu;
    pthread_t hilo_timer;
    bool timer_activo;
} t_cpu_timer;

// El timer
typedef struct {
    int socket_cpu;
    uint32_t quantum_ms;
} t_args_timer;

void inicializar_ks_cpu_manager();
void agregar_cpu_libre(int socket_cpu);
void* hilo_dispatcher(void* arg);
void manejar_mutex_create(int socket_cpu, t_pcb* proceso); 
void manejar_syscall_io_cpu(int socket_cpu, t_pcb* proceso);
void manejar_exit(int socket_cpu, t_pcb* proceso);
void manejar_iniciar_proceso(int socket_cpu, int socket_kernel_memory_operaciones);
void manejar_tick_progress(int socket_cpu, t_pcb* proceso);
void manejar_fin_quantum(int socket_cpu, t_pcb* proceso);

#endif
#ifndef PLANIFICADOR_H
#define PLANIFICADOR_H

#include "pcb.h"
#include "utils/mensajes.h"
#include <commons/collections/queue.h>
#include <commons/config.h>
#include <semaphore.h>
#include <pthread.h>

extern t_log* logger;
extern t_config* config;

extern int socket_kernel_memory_operaciones;
extern pthread_mutex_t mutex_socket_km_operaciones;

extern uint32_t proximo_pid;
extern pthread_mutex_t mutex_pid;

extern sem_t sem_compactacion;

extern bool desalojo_por_compactacion;
extern pthread_mutex_t mutex_desalojo_compactacion;

extern sem_t sem_desalojo_ok;

extern char* algoritmo;
extern char** queues_algoritmos;

extern int cant_prioridades;

// Para socket_cpu -> hilo timer activo
typedef struct {
    int socket_cpu;
    pthread_t hilo_timer;
} t_cpu_timer;

typedef struct {
    int socket_cpu;
    uint32_t quantum_ms;
} t_args_timer;

void inicializar_ks_cpu_manager();

void agregar_cpu_libre(int socket_cpu);

void* hilo_dispatcher(void* arg);

void manejar_tick_progress(int socket_cpu, t_pcb* proceso);
void manejar_syscall_io_cpu(int socket_cpu, t_pcb* proceso);
void manejar_syscall_mutex_create(int socket_cpu, t_pcb* proceso);
void manejar_syscall_mem_alloc(int socket_cpu, t_pcb* proceso);
void manejar_syscall_mem_free(int socket_cpu, t_pcb* proceso);
void manejar_syscall_exit(int socket_cpu, t_pcb* proceso);
void manejar_iniciar_proceso(int socket_cpu, int socket_kernel_memory_operaciones, t_pcb* llamador);

void recrear_timer(int socket_cpu, t_pcb* proceso);

void cancelar_timer(int socket_cpu);

void marcar_interrupcion(int socket_cpu);

#endif
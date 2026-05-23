#ifndef MUTEX_MANAGER_H
#define MUTEX_MANAGER_H

#include "pcb.h"
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <pthread.h>

typedef struct {
    char* nombre;
    t_pcb* duenio;
    t_queue* bloqueados;
    pthread_mutex_t mutex_interno;
} t_mutex_kernel;

extern t_list* lista_mutexes;

// Inicialización
void inicializar_mutexes();

// Manejo interno
t_mutex_kernel* crear_mutex(char* nombre);
t_mutex_kernel* buscar_mutex(char* nombre);

// Lock / Unlock
bool mutex_lock(char* nombre, t_pcb* proceso);
void mutex_unlock(char* nombre);

// Handlers CPU -> Kernel
void manejar_mutex_lock(int socket_cpu, t_pcb* proceso);
void manejar_mutex_unlock(int socket_cpu, t_pcb* proceso);

#endif
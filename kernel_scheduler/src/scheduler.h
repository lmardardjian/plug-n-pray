#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <commons/collections/queue.h>
#include <pthread.h>
#include <semaphore.h>
#include "pcb.h"

// VAriables globales para la cola y sincronización
extern t_queue *cola_ready;
extern t_queue *cola_block;
extern pthread_mutex_t mutex_ready;
extern sem_t sem_procesos_en_ready;
extern pthread_mutex_t mutex_block;
extern sem_t sem_procesos_en_block;
extern pthread_mutex_t mutex_running;
extern sem_t sem_procesos_en_running;

void inicializar_planificador();

void agregar_a_running(t_list* lista_running, uint32_t pid);

void quitar_de_running(t_list* lista_running, uint32_t pid);

void agregar_a_ready(t_queue* cola_ready, uint32_t pid);

void quitar_de_ready(t_queue* cola_ready, uint32_t pid);

void agregar_a_block(t_queue* cola_block, uint32_t pid);

void quitar_de_block(t_queue* cola_block, uint32_t pid);

t_pcb* obtener_siguiente_proceso();

#endif
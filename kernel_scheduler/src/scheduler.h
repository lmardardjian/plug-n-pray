#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "pcb.h"
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>

extern char** queues_algoritmos;

// Colas y listas
extern t_queue *cola_ready;
extern t_queue *lista_block;
extern t_list *lista_exec;

//Mutex 
extern pthread_mutex_t mutex_ready;
extern pthread_mutex_t mutex_block;
extern pthread_mutex_t mutex_exec;

extern sem_t sem_procesos_en_ready;

void inicializar_planificador();

void agregar_a_ready(t_pcb* proceso);
t_pcb* obtener_siguiente_proceso();

void agregar_a_block(t_pcb* proceso);
t_pcb* quitar_de_block(uint32_t pid);

void agregar_a_exec(t_pcb* proceso);
void quitar_de_exec(uint32_t pid);

#endif
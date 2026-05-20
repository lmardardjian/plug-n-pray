#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <commons/collections/queue.h>
#include <pthread.h>
#include <semaphore.h>
#include "pcb.h"

// VAriables globales para la cola y sincronización
extern t_queue* cola_ready;
extern pthread_mutex_t mutex_ready;
extern sem_t sem_procesos_en_ready;

void inicializar_planificador();
void agregar_a_ready(t_pcb* proceso);
t_pcb* obtener_siguiente_proceso();

#endif
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "pcb.h"
#include "utils/hilos.h"
#include "utils/mensajes.h"
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>

extern char** queues_algoritmos;

extern int cant_prioridades;

extern int socket_kernel_memory;
extern pthread_mutex_t mutex_socket_km;

extern t_log* logger;
extern uint32_t suspension_timeout;


// Colas y listas
extern t_queue *cola_ready;
extern t_queue *lista_block;
extern t_list *lista_exec;
extern t_list* lista_susp_ready;
extern t_list* lista_susp_block;

//Mutexes 
extern pthread_mutex_t mutex_ready;
extern pthread_mutex_t mutex_block;
extern pthread_mutex_t mutex_exec;
extern pthread_mutex_t mutex_susp_ready;
extern pthread_mutex_t mutex_susp_block;

//semáforo productor-consumidor de cola_ready
extern sem_t sem_procesos_en_ready;

void inicializar_ks_planificador();

void agregar_a_ready(t_pcb* proceso);
t_pcb* obtener_siguiente_proceso();

void agregar_a_block(t_pcb* proceso);
t_pcb* quitar_de_block(uint32_t pid);

void agregar_a_exec(t_pcb* proceso);
void quitar_de_exec(uint32_t pid);

void agregar_a_susp_ready(t_pcb* proceso);
t_pcb* quitar_de_susp_ready(uint32_t pid);

void agregar_a_susp_block(t_pcb* proceso);
t_pcb* quitar_de_susp_block();

void* hilo_suspension(void* arg);
void intentar_reanudar_proceso();

#endif
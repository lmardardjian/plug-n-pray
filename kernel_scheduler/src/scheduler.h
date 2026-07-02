#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "pcb.h"
#include "utils/mensajes.h"
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>

extern t_queue** colas_ready;
extern t_list* lista_block;
extern t_list* lista_exec;
extern t_list** listas_susp_ready;
extern t_list** listas_susp_block;

extern pthread_mutex_t mutex_ready;
extern pthread_mutex_t mutex_block;
extern pthread_mutex_t mutex_exec;
extern pthread_mutex_t mutex_susp_ready;
extern pthread_mutex_t mutex_susp_block;

extern sem_t sem_procesos_en_ready;

extern int socket_kernel_memory_operaciones;
extern pthread_mutex_t mutex_socket_km_operaciones;

extern int cant_prioridades;

extern bool hay_desalojo_cmn;

extern uint32_t suspension_timeout;

extern char* algoritmo;

extern t_log* logger;

void inicializar_ks_planificador();

void agregar_a_ready(t_pcb* proceso);
void agregar_al_principio_de_ready(t_pcb* proceso);
void reinsertar_al_principio_de_ready(t_pcb* proceso);
t_pcb* quitar_de_ready_por_pid(uint32_t pid);
t_pcb* obtener_siguiente_proceso();

void agregar_a_block(t_pcb* proceso);
t_pcb* quitar_de_block(uint32_t pid);

void agregar_a_exec(t_pcb* proceso);
void quitar_de_exec(uint32_t pid);

void agregar_a_susp_ready(t_pcb* proceso);
t_pcb* quitar_de_susp_ready_por_pid(uint32_t pid);

void agregar_a_susp_block(t_pcb* proceso);
t_pcb* quitar_de_susp_block();
t_pcb* quitar_de_susp_block_por_pid(uint32_t pid);

void* hilo_suspension(void* arg);
void intentar_reanudar_proceso();
void registrar_cpu_proceso(int socket_cpu_ejecutando, uint32_t pid);

int obtener_socket_cpu_de(uint32_t pid);
int32_t obtener_pid_de_cpu(int socket_cpu);

#endif
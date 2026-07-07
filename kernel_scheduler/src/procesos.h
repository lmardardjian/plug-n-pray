#ifndef PROCESOS_H
#define PROCESOS_H

#include "pcb.h"
#include <commons/collections/list.h>
#include <commons/log.h>
#include <pthread.h>

extern t_list* p_activos_global;
extern pthread_mutex_t mutex_p_activos;

extern int socket_kernel_memory_operaciones;
extern pthread_mutex_t mutex_socket_km_operaciones;

t_pcb *crear_pcb (uint32_t pid, uint32_t prioridad);
t_pcb* encontrar_proceso_global(uint32_t pid);

void destruir_pcb(void* pcb);
void destruir_todos_global();
t_pcb* remover_de_activos_global(uint32_t pid);

void cambiar_estado(t_pcb *proceso, t_estado nuevo_estado, t_log* logger);

#endif
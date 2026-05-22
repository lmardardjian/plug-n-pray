#ifndef PROCESOS_H
#define PROCESOS_H

#include "pcb.h"
#include <commons/collections/list.h>
#include <commons/log.h>
#include <stdbool.h>

extern t_list* p_activos_global;

t_pcb *crear_pcb (uint32_t pid, uint32_t prioridad);
void destruir_pcb(void* pcb);
void destruir_todos(t_list* procesos);
t_pcb* encontrar_proceso(t_list* procesos, uint32_t pid);
void cambiar_estado(t_pcb *proceso, t_estado nuevo_estado, t_log* logger);

#endif
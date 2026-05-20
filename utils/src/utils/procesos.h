#ifndef PROCESOS_H
#define PROCESOS_H
#include "kernel_scheduler/src/pcb.h"
#include <commons/collections/list.h>


void crear_proceso(uint32_t pid);
void matar_proceso(uint32_t pid);
t_pcb *encontrar_proceso(t_list *procesos_activos, uint32_t pid);
void cambiar_estado(t_pcb *proceso_a_cambiar, t_estado nuevo_estado);


#endif
#include "/home/utnso/Documents/tp-2026-1c-Bobby-Tables/kernel_scheduler/src/pcb.h"
#include "./procesos.h"
#include <stdint.h>

t_pcb *crear_proceso(uint32_t pid_asignado){
    t_pcb *proceso = malloc(sizeof(t_pcb));
    proceso->pid = pid_asignado;
    proceso->pc = 0;
    proceso->estado = ESTADO_NEW;
    return proceso;
}

void matar_proceso(uint32_t pid_asignado) {
    t_pcb *proceso = encontrar_proceso(p_activos_global, pid_asignado);
    if(proceso != NULL) {
        free(proceso->pid);
        free(proceso->pc);
        free(proceso->estado);
        //liberar cada elemento de la estructura proceso
    }
}

void kill_all_processes (void *ptr) {
    t_pcb *proceso = (t_pcb *) ptr;
    free(proceso->pid);
    free(proceso->pc);
    free(proceso->estado);
}

//esta solución está basada en el manual de uso de list_find pero no es portable.
//para que sea portable hay que usar una variable global para el pid_a_buscar y
//hacer uso de un semáforo mutex.

bool es_proceso(void *ptr) {
    t_pcb *proceso = (t_pcb *) ptr;
    return ((proceso->pid) == pid_a_buscar);
}

t_pcb * encontrar_proceso(t_list *procesos_activos, uint32_t pid_a_buscar){
    
    return list_find(p_activos_global, es_proceso);
}

void cambiar_estado(t_pcb *proceso, t_estado nuevo_estado){
    proceso->estado = nuevo_estado;
}
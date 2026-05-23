#include "procesos.h"

t_list* p_activos_global = NULL; // podríamos hacer uso de las funciones de listas?

/*
typedef struct {
    uint32_t pid; //esta estructura está medio al pedo
} t_busqueda;


static bool mismo_pid(void* elemento, void* contexto) {
    t_pcb* proceso = (t_pcb*) elemento;
    t_busqueda* busqueda = (t_busqueda*) contexto;
    return proceso->pid == busqueda->pid;
}*/


t_pcb* crear_pcb(uint32_t pid, uint32_t prioridad) {
    t_pcb* proceso = malloc(sizeof(t_pcb));
    if (proceso == NULL) return NULL;

    proceso -> pid = pid;
    proceso -> estado = ESTADO_NEW;
    proceso -> prioridad = prioridad;
    proceso -> prioridad_original = prioridad;

    return proceso;
}

void destruir_pcb(void* ptr) {
    t_pcb* elem = (t_pcb*) ptr;
    free(ptr);
}

void destruir_todos(t_list* procesos) {
    list_destroy_and_destroy_elements(procesos, destruir_pcb);
}

t_pcb* encontrar_proceso(t_list* procesos, uint32_t pid) {
    for (int i = 0; i < list_size(procesos); i++) {
        t_pcb* proceso = list_get(procesos, i);
        if (proceso->pid == pid) return proceso;
    }
    return NULL;
}

static char* estado_to_string(t_estado estado) {
    switch (estado) {
        case ESTADO_NEW:        
            return "NEW";
            
        case ESTADO_READY:      
            return "READY";

        case ESTADO_EXEC:       
            return "EXEC";

        case ESTADO_BLOCK:      
            return "BLOCK";

        case ESTADO_EXIT:       
            return "EXIT";

        case ESTADO_SUSP_READY: 
            return "SUSP_READY";

        case ESTADO_SUSP_BLOCK: 
            return "SUSP_BLOCK";

        default:                
            return "DESCONOCIDO";
    }
}

void cambiar_estado(t_pcb *proceso, t_estado nuevo_estado, t_log* logger) {
    log_info(logger, "## (%d) Pasa del estado %s al estado %s", proceso->pid, estado_to_string(proceso->estado), estado_to_string(nuevo_estado));
    proceso -> estado = nuevo_estado;
}
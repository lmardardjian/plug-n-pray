#include "procesos.h"

t_pcb* crear_pcb(uint32_t pid, uint32_t prioridad) {
    t_pcb* proceso = malloc(sizeof(t_pcb));
    if (proceso == NULL) return NULL; //falta logger?

    proceso -> pid = pid;
    proceso -> estado = ESTADO_NEW;
    proceso -> prioridad = prioridad;
    proceso -> prioridad_original = prioridad;

    return proceso;
}

t_pcb* encontrar_proceso(t_list* procesos, uint32_t pid) {

    pthread_mutex_lock(&mutex_p_activos);

    t_pcb* resultado = NULL;
    for (int i = 0; i < list_size(procesos); i++) {
        t_pcb* proceso = list_get(procesos, i);
        if (proceso->pid == pid) {
        resultado = proceso;
        break;
        }
    }

    pthread_mutex_unlock(&mutex_p_activos);

    return resultado;
}

void destruir_pcb(void* ptr) {
    t_pcb* elem = (t_pcb*) ptr;
    free(ptr);
}

void destruir_todos(t_list* procesos) {//podría usar directamente la lista de procesos global y no recibir parámetro? para evitar confusiones

    pthread_mutex_lock(&mutex_p_activos);

    list_destroy_and_destroy_elements(procesos, destruir_pcb); 

    pthread_mutex_unlock(&mutex_p_activos);
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

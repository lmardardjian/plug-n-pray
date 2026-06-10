#include "procesos.h"

t_pcb* crear_pcb(uint32_t pid, uint32_t prioridad) {
    t_pcb* proceso = malloc(sizeof(t_pcb));
    proceso -> pid = pid;
    proceso -> estado = ESTADO_NEW;
    proceso -> prioridad = prioridad;
    proceso -> prioridad_original = prioridad;
    pthread_mutex_init(&proceso->mutex_estado, NULL);

    return proceso;
}

t_pcb* encontrar_proceso_global(uint32_t pid) {

    pthread_mutex_lock(&mutex_p_activos);

    t_pcb* resultado = NULL;
    int tamanio = list_size(p_activos_global);
    for (int i = 0; i < tamanio; i++) {
        t_pcb* proceso = list_get(p_activos_global, i);
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

void destruir_todos_global() { //cambia cuando KM esté completo

    pthread_mutex_lock(&mutex_p_activos);

    list_destroy_and_destroy_elements(p_activos_global, destruir_pcb); 

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

    pthread_mutex_lock(&proceso->mutex_estado);

    log_info(logger, "## (%d) Pasa del estado %s al estado %s", proceso->pid, estado_to_string(proceso->estado), estado_to_string(nuevo_estado));
    proceso -> estado = nuevo_estado;

    pthread_mutex_unlock(&proceso->mutex_estado);
}

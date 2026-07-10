#include "procesos.h"
#include "scheduler.h"
#include "utils/conexion.h"
#include "utils/mensajes.h"
#include <stdlib.h>

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

    if (p_activos_global != NULL) {
        int tamanio = list_size(p_activos_global);
        for (int i = 0; i < tamanio; i++) {
            t_pcb* proceso = list_get(p_activos_global, i);
            if (proceso->pid == pid) {
            resultado = proceso;
            break;
            }
        }
    }
    pthread_mutex_unlock(&mutex_p_activos);

    return resultado;
}

void destruir_pcb(void* ptr) {
    t_pcb* elem = (t_pcb*) ptr;
    pthread_mutex_destroy(&elem->mutex_estado);
    free(ptr);
}

void destruir_todos_global() {

    pthread_mutex_lock(&mutex_p_activos);

    int tamanio = list_size(p_activos_global);
    for (int i = 0; i < tamanio; i++) {
        t_pcb* proceso = list_get(p_activos_global, i);

        pthread_mutex_lock(&mutex_socket_km_operaciones);

        enviar_opcode(socket_kernel_memory_operaciones, KM_FINALIZAR_PROCESO);
        enviar_uint32(socket_kernel_memory_operaciones, proceso->pid);

        op_code ack;
        if (recibir_opcode(socket_kernel_memory_operaciones, &ack) <= 0) {
            log_error(logger, "## PID %u: no se pudo confirmar liberación de memoria con Kernel Memory (conexión perdida)", proceso->pid);
            
            pthread_mutex_unlock(&mutex_socket_km_operaciones);

            break;
        }

        log_info(logger, "## (%u) finalizó su ejecución con motivo de BSOD", proceso->pid);

        pthread_mutex_unlock(&mutex_socket_km_operaciones);
    }

    list_destroy_and_destroy_elements(p_activos_global, destruir_pcb);
    p_activos_global = NULL;

    pthread_mutex_unlock(&mutex_p_activos);
}

t_pcb* remover_de_activos_global(uint32_t pid) {

    pthread_mutex_lock(&mutex_p_activos);

    t_pcb* resultado = NULL;
    int tamanio = list_size(p_activos_global);
    for (int i = 0; i < tamanio; i++) {
        t_pcb* proceso = list_get(p_activos_global, i);
        if (proceso->pid == pid) {
            resultado = list_remove(p_activos_global, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_p_activos);

    return resultado;
}

bool rescatar_proceso_de_cpu_desconectada(uint32_t pid, char* id_cpu, t_log* logger) {

    pthread_mutex_lock(&mutex_p_activos);

    if (p_activos_global == NULL) {

        pthread_mutex_unlock(&mutex_p_activos);
        
        return false;
    }

    t_pcb* rescatado = NULL;
    int tamanio = list_size(p_activos_global);
    for (int i = 0; i < tamanio; i++) {
        t_pcb* p = list_get(p_activos_global, i);
        if (p->pid == pid) {
            rescatado = p;
            break;
        }
    }

    if (rescatado == NULL) {

        pthread_mutex_unlock(&mutex_p_activos);

        return false;
    }

    log_warning(logger, "## CPU %s desconectada con PID %d en ejecución. Proceso rescatado a READY.", id_cpu, pid);
    
    quitar_de_exec(pid);
    cambiar_estado(rescatado, ESTADO_READY, logger);
    agregar_a_ready(rescatado);

    pthread_mutex_unlock(&mutex_p_activos);

    return true;
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

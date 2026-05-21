#include "pcb.h"
#include "procesos.h"
#include <stdlib.h>
#include <commons/log.h>

t_list* p_activos_global = NULL;


typedef struct {
    uint32_t pid;
} t_busqueda;

static bool mismo_pid(void* elemento, void* contexto) {
    t_pcb* proceso = (t_pcb*) elemento;
    t_busqueda* busqueda = (t_busqueda*) contexto;
    return proceso->pid == busqueda->pid;
}


t_pcb* crear_pcb(uint32_t pid, uint32_t prioridad) {
    t_pcb* proceso = malloc(sizeof(t_pcb));
    if (proceso == NULL) return NULL;

    proceso -> pid                   = pid;
    proceso -> estado                = ESTADO_NEW;
    proceso -> prioridad             = prioridad;
    proceso -> prioridad_original    = prioridad;

    return proceso;
}

void destruir_pcb(void* ptr) {
    free(ptr);
}

void destruir_todos(t_list* procesos) {
    list_destroy_and_destroy_elements(procesos, destruir_pcb);
}

/*t_pcb* encontrar_proceso(t_list* procesos, uint32_t pid) {
    t_busqueda busqueda = { .pid = pid };
    return list_find_with_context(procesos, mismo_pid, &busqueda);
}
*/

void cambiar_estado(t_pcb *proceso, t_estado nuevo_estado, t_log* logger) {
    /*
    char* nombres[] = {
        "NEW", "READY", "EXEC", "BLOCK", "EXIT", "SUSP_READY", "SUSP_BLOCK"
    };
    log_info(logger, "## (%d) Pasa del estado %s al estado %s",
             proceso->pid,
             nombres[proceso->estado],
             nombres[nuevo_estado]);
    */
    proceso -> estado = nuevo_estado;
}
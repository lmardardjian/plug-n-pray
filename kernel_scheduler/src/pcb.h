#ifndef PCB_H
#define PCB_H
#include <stdint.h>

//Enumerador para los estados del proceso
typedef enum {
    ESTADO_NEW,
    ESTADO_READY,
    ESTADO_EXEC,
    ESTADO_BLOCK,
    ESTADO_EXIT,
    ESTADO_SUSP_READY,
    ESTADO_SUSP_BLOCK
} t_estado;


typedef struct {
    uint32_t pid;       // Identificador único
    t_estado estado;    // Estado actual 
    uint32_t prioridad;
    uint32_t prioridad_original;
    long tiempo_susp;   // "Long" porque el tiempo está en milisegundos
} t_pcb;

#endif
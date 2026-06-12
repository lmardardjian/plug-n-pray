#ifndef PCB_H
#define PCB_H
#include <stdint.h>
#include <pthread.h>
#include <commons/temporal.h>

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
    uint32_t pid; //identificador único.
    t_estado estado; //estado actual.
    t_temporal* tiempo_susp; //estructura inicializada cuando el proceso entra en block. se libera cuando sale de block.
    pthread_mutex_t mutex_estado; //mutex para proteger el cambio de estado.
    uint32_t prioridad_original;
    uint32_t prioridad;
} t_pcb;

#endif
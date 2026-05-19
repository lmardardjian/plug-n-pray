#ifndef PCB_H
#define PCB_H
#include <stdint.h>

//Enumerador para los estados del proceso
typedef enum {
    ESTADO_NEW,
    ESTADO_READY,
    ESTADO_EXEC,
    ESTADO_BLOCK,
    ESTADO_EXIT
    // ESTADO_SUSP_READY
    // ESTADP_SUSP_BLOCK
} t_estado;


typedef struct {
    uint32_t pid;       // Identificador único
    uint32_t pc;        // Program Counter
    t_estado estado;    // Estado actual 
    //Luego añadimos CPU regs y prioridades
} t_pcb;

#endif
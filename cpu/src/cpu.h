#ifndef CPU_H
#define CPU_H

#include <commons/log.h>
#include "utils/mensajes.h"
#include "utils/constantes.h"

// PID actual que está ejecutando esta CPU
// (global para poder usarlo en logs desde cualquier lado)
extern int pid_actual;

// Tamaño máximo de segmento (SEGMENT_MAX_SIZE), leído del config.
// Lo necesita la MMU para traducir direcciones lógicas a físicas.
extern uint32_t tam_max_segmento;

// Funciones principales
void ciclo_instruccion(int fd_scheduler, int fd_memory, t_log* logger);

// Helpers de registros
uint32_t leer_registro(t_contexto* ctx, char* nombre);
void     escribir_registro(t_contexto* ctx, char* nombre, uint32_t valor);

typedef struct {
    tipo_instruccion tipo;
    char param1[MAX_PARAM_INSTRUCCION_LEN];
    char param2[MAX_PARAM_INSTRUCCION_LEN];
} t_instruccion;

t_instruccion decode(char* texto);

#endif
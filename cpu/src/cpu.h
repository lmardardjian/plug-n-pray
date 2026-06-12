#ifndef CPU_H
#define CPU_H

#include <commons/log.h>
#include <utils/mensajes.h>  // donde está t_contexto y op_code

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
    char param1[32];
    char param2[32];
} t_instruccion;

t_instruccion decode(char* texto);

#endif
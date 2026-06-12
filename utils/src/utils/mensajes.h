#ifndef MENSAJES_H
#define MENSAJES_H

#include<stdint.h>
#include <commons/collections/list.h>

typedef enum {
    MENSAJE,

    //mensajes io
    IO_EJECUTAR,

    //mensajes kernel memory
    KM_CREAR_PROCESO,
    KM_PEDIR_INSTRUCCION,
    KM_PEDIR_CONTEXTO,
    KM_ACTUALIZAR_CONTEXTO,
    KM_MEM_READ,
    KM_MEM_WRITE,
    KM_ESPACIO_LIBRE,
    KM_SUSPENDER_PROCESO,
    KM_REANUDAR_PROCESO,
    KM_BSOD,

    //respuestas
    RESPUESTA_OK,
    RESPUESTA_ERROR,

    // mensajes cpu -> kernel scheduler
    KS_TICK_PROGRESS_CONTINUE,
    KS_FIN_QUANTUM,
    KS_SYSCALL_IO,
    KS_MUTEX_LOCK,
    KS_MUTEX_UNLOCK,
    KS_MUTEX_CREATE,
    KS_EXIT,
    KS_INIT_PROC,

    //mensajes memory stick
    //MS_RESERVAR,
    //MS_LIBERAR,
    MS_LEER,
    MS_ESCRIBIR,
} op_code;

typedef enum {
    MODULO_KERNEL_MEMORY = 1,
    MODULO_KERNEL_SCHEDULER = 2,
    MODULO_CPU = 3,
    MODULO_IO = 4,
    MODULO_MEMORY_STICK = 5,
    MODULO_SWAP = 6,
} t_modulo;

typedef enum {
    TIPO_IO_SLEEP,
    TIPO_IO_STDIN,
    TIPO_IO_STDOUT
} tipo_io;

typedef struct {
    uint32_t id_segmento;
    uint32_t base;
    uint32_t limite;
} t_segmento;

typedef struct {
    uint32_t pc;

    uint8_t ax;
    uint8_t bx;
    uint8_t cx;
    uint8_t dx;

    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    uint32_t si;
    uint32_t di;

    t_list* tabla_segmentos;
} t_contexto;

// memory stick para kernel memory
typedef struct {
    int socket;
    uint32_t tamanio;
} t_memory_stick;

// instrucciones que entiende la cpu
typedef enum {
    INST_NOOP,
    INST_SET,
    INST_SUM,
    INST_SUB,
    INST_JNZ,

    INST_MOV_IN,
    INST_MOV_OUT,
    INST_COPY_MEM,

    INST_MUTEX_CREATE,
    INST_MUTEX_LOCK,
    INST_MUTEX_UNLOCK,

    INST_MEM_ALLOC,
    INST_MEM_FREE,

    INST_SLEEP,
    INST_STDOUT,
    INST_STDIN,

    INST_INIT_PROC,
    INST_EXIT,

    INST_DESCONOCIDA
} tipo_instruccion;

#endif
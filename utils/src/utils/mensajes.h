#ifndef MENSAJES_H
#define MENSAJES_H

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

    // mensajes cpu -> kernel scheduler
    KS_FIN_QUANTUM,
    KS_SYSCALL_IO,
    KS_EXIT,

    //respuestas
    RESPUESTA_OK,
    RESPUESTA_ERROR
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

/* PARA MAS ADELANTE EN EL TP
typedef struct {
    uint32_t id_segmento;
    uint32_t base;
    uint32_t limite;
} t_segmento;
*/
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

    // t_list* tabla_segmentos; PARA MAS ADELANTE EN EL TP
} t_contexto;

#endif
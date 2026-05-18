#ifndef MENSAJES_H
#define MENSAJES_H

typedef enum {
    MENSAJE,

    //mensajes io
    IO_EJECUTAR,
    IO_FINALIZADA,
    IO_ERROR,
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


#endif
#ifndef CONSTANTES_H
#define CONSTANTES_H

#define BUFFER_SIZE            256

#define PRIORIDAD_MAXIMA       0   // menor número = mayor prioridad

// Buffer para nombres de mutex
#define MAX_NOMBRE_MUTEX      64

// Buffer para el nombre del archivo de log
#define MAX_NOMBRE_LOG        64

// Buffer para el identificador de CPU que viaja por red
#define MAX_ID_CPU            32

// Tamaño máximo del string de un parámetro numérico de IO
#define MAX_PARAM_IO_LEN      20

// Valores de respuesta al check_interrupt de la CPU
#define HAY_INTERRUPCION       1
#define SIN_INTERRUPCION       0

#endif
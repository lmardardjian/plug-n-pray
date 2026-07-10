#ifndef CONSTANTES_H
#define CONSTANTES_H

// Menor número = mayor prioridad.
#define PRIORIDAD_MAXIMA                0

// Valores de respuesta al check_interrupt de la CPU.
#define SIN_INTERRUPCION                0
#define HAY_INTERRUPCION                1

// Tamaño máximo del string de un parámetro numérico de IO.
#define MAX_PARAM_IO_LEN                20

// Tamaño máximo de clave de diccionario.
#define MAX_PID_KEY_LEN                 20

// Buffer para el identificador de CPU que viaja por red.
#define MAX_ID_CPU                      32

// Tamaño máximo de parámetro de instrucción.
#define MAX_PARAM_INSTRUCCION_LEN       32

// Buffer para nombres de mutex.
#define MAX_NOMBRE_MUTEX                64

// Buffer para el nombre del archivo de log.
#define MAX_NOMBRE_LOG                  64

#define BUFFER_SIZE                     256

// Conversión de milisegundos a microsegundos para usleep().
#define MS_A_US                         1000 

#endif
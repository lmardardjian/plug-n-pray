#ifndef IO_H
#define IO_H

#include <commons/log.h>
#include "utils/mensajes.h"

tipo_io get_tipo_io(char* tipo);

void ejecutar_sleep(int pid, char* mensaje, t_log* logger);

void ejecutar_stdout(int pid, char* mensaje, t_log* logger);

void ejecutar_stdin(int pid, int conexion, char* mensaje, t_log* logger);

#endif
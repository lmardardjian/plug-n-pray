#ifndef IO_H
#define IO_H

#include <commons/log.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int get_tipo_io(char* tipo);

void ejecutar_sleep(int pid, char* mensaje, t_log* logger);

void ejecutar_stdout(int pid, char* mensaje, t_log* logger);

void ejecutar_stdin(int pid, int conexion, char* mensaje, t_log* logger);

#endif
#ifndef CICLO_H
#define CICLO_H

#include <commons/log.h>
#include <utils/mensajes.h>
#include <stdint.h>

int pid_actual;
static char* fetch(int fd_memory, uint32_t pc, t_log* logger);
static int execute(char* instruccion, t_contexto* ctx, int fd_scheduler, int fd_memory, t_log* logger);
static int hay_interrupcion(int fd_scheduler);
void ciclo_instruccion(int fd_scheduler, int fd_memory, t_log* logger);

#endif
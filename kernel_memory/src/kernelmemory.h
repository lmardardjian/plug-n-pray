#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include "utils/mensajes.h"
#include <commons/memory.h>
#include <commons/config.h>
#include <commons/log.h>
#include <string.h>
typedef struct {
    int socket;
    t_log* logger;
    t_config* config;
    t_dictionary* procesos;
    t_list* memory_sticks;
} t_args_cliente;

typedef struct {
    u_int32_t pid;
    t_list* instrucciones;
    t_contexto contexto;
} t_proceso_memoria;

t_list* leer_instrucciones(char* path);
void* atender_cliente(void* arg);
void crear_proceso(int cliente, t_dictionary* procesos, t_log* logger);
void enviar_instruccion(int cliente, t_dictionary* procesos, t_log* logger);
void responder_espacio_libre(int cliente, t_log* logger);
void responder_mem_write(int cliente, t_log* logger);
void responder_mem_read(int cliente, t_log* logger);
void actualizar_contexto(int cliente, t_dictionary* procesos, t_log* logger);
void enviar_contexto(int cliente, t_dictionary* procesos, t_log* logger);

#endif
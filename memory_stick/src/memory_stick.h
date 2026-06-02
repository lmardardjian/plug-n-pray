#ifndef MEMORY_STICK_H
#define MEMORY_STICK_H

#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <commons/log.h>

typedef struct {
    void* memoria;
    uint32_t tamanio;
    pthread_mutex_t mutex_memoria;
} t_memory_stick_local;

typedef struct {
    int socket;
    t_memory_stick_local* stick;
    t_log* logger;
} t_args_cpu;

int escribir_memoria(t_memory_stick_local* stick, uint32_t direccion, void* datos, uint32_t tamanio);
int leer_memoria(t_memory_stick_local* stick, uint32_t direccion, void* destino, uint32_t tamanio);

void atender_lectura(int cliente, t_memory_stick_local* stick, t_log* logger);
void atender_escritura(int cliente, t_memory_stick_local* stick, t_log* logger);

void* atender_cpu(void* arg);

#endif


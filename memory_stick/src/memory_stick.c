#include "memory_stick.h"
#include "utils/conexion.h"
#include "utils/constantes.h"
#include <string.h>
#include <stdlib.h>

int escribir_memoria(t_memory_stick_local* stick, uint32_t direccion, void* datos, uint32_t tamanio)
{
    if(direccion + tamanio > stick->tamanio)
        return -1;

    pthread_mutex_lock(&stick->mutex_memoria);

    memcpy((char*)stick->memoria + direccion, datos, tamanio);

    pthread_mutex_unlock(&stick->mutex_memoria);

    return 0;
}

int leer_memoria(t_memory_stick_local* stick, uint32_t direccion, void* destino, uint32_t tamanio)
{
    if(direccion + tamanio > stick->tamanio)
        return -1;

    pthread_mutex_lock(&stick->mutex_memoria);

    memcpy(destino, (char*)stick->memoria + direccion, tamanio);

    pthread_mutex_unlock(&stick->mutex_memoria);

    return 0;
}

void atender_escritura(int cliente, t_memory_stick_local* stick, t_log* logger, int memory_delay)
{
    usleep(memory_delay * MS_A_US);

    uint32_t direccion;
    uint32_t tamanio;

    recibir_uint32(cliente, &direccion);
    recibir_uint32(cliente, &tamanio);

    void* buffer = malloc(tamanio);

    recibir_buffer(cliente, buffer, tamanio);

    if(escribir_memoria(stick, direccion, buffer, tamanio) == 0) {
        enviar_opcode(cliente, RESPUESTA_OK);
        log_info(logger, "## Escritura de %u bytes", tamanio);
    }
    else {
        enviar_opcode(cliente, RESPUESTA_ERROR);
    }

    free(buffer);
}

void atender_lectura(int cliente, t_memory_stick_local* stick, t_log* logger, int memory_delay)
{
    usleep(memory_delay * MS_A_US);

    uint32_t direccion;
    uint32_t tamanio;
    recibir_uint32(cliente, &direccion);
    recibir_uint32(cliente, &tamanio);

    void* buffer = malloc(tamanio);
 
    if (leer_memoria(stick, direccion, buffer, tamanio) == 0) {
        enviar_opcode(cliente, RESPUESTA_OK);
        enviar_buffer(cliente, buffer, tamanio);
        log_info(logger, "## Lectura de %u bytes", tamanio);
    } else {
        enviar_opcode(cliente, RESPUESTA_ERROR);
    }
    free(buffer);
}

void* atender_cpu(void* arg) //OJO CON EL NOMBRE ya hay una función atender_cpu
{
    t_args_cpu* args = (t_args_cpu*) arg;

    int cliente = args->socket;
    t_memory_stick_local* stick = args->stick;
    t_log* logger = args->logger;
    int memory_delay = args->memory_delay;

    free(args);

    while(1)
    {
        op_code operacion;

        if(recibir_opcode(cliente, &operacion) <= 0) //DUDA falta log error
            break;

        switch(operacion)
        {
            case MS_LEER:
                atender_lectura(cliente, stick, logger, memory_delay);
                break;

            case MS_ESCRIBIR:
                atender_escritura(cliente, stick, logger, memory_delay);
                break;

            default:
                log_error(logger, "Operacion desconocida");
                break;
        }
    }

    cerrar_conexion(cliente, logger);
    return NULL;
}
#include "utils/hilos.h"
#include "memory_stick.h"
#include "utils/conexion.h"
#include <commons/config.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    if(argc < 3) {
        printf("Uso: ./memory_stick [config] [tamanio]\n");
        return EXIT_FAILURE;
    }

    t_config* config = config_create(argv[1]);
    if(config == NULL) {
        printf("Error al leer config\n");
        return EXIT_FAILURE;
    }

    t_memory_stick_local* stick = malloc(sizeof(t_memory_stick_local));
    stick->tamanio = atoi(argv[2]);
    stick->memoria = malloc(stick->tamanio);
    if(stick->memoria == NULL) {
        printf("No se pudo reservar memoria\n");
        free(stick);
        config_destroy(config);
        return EXIT_FAILURE;
    }
    pthread_mutex_init(&stick->mutex_memoria, NULL);

    char* ip_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");

    t_log* logger = log_create("memory_stick.log", "MEMORY_STICK", 1, log_level_from_string(log_level_str));
    int memory_delay = config_get_int_value(config, "MEMORY_DELAY");

    int socket_kernel_memory_operaciones = conectar_a_modulo(logger, ip_memory, puerto_memory, "Kernel Memory");
    if(socket_kernel_memory_operaciones == -1) {
        free(stick->memoria);
        free(stick);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    if(handshake_cliente(socket_kernel_memory_operaciones, logger, MODULO_MEMORY_STICK) != 0) {
        cerrar_conexion(socket_kernel_memory_operaciones, logger);
        free(stick->memoria);
        free(stick);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    log_info(logger, "## Conectado a Kernel Memory");

    enviar_uint32(socket_kernel_memory_operaciones, stick->tamanio);

    // A partir de acá, Kernel Memory usa ESTE MISMO socket (el de registro)
    // para mandar MS_LEER/MS_ESCRIBIR. No hay que abrir un servidor nuevo:
    // hay que quedarse escuchando opcodes sobre esta misma conexión.
    log_info(logger, "## Memory Stick listo, esperando operaciones de Kernel Memory...");

    while(1)
    {
        op_code operacion;

        if(recibir_opcode(socket_kernel_memory_operaciones, &operacion) <= 0) {
            log_info(logger, "## Kernel Memory se desconectó");
            break;
        }

        switch(operacion)
        {
            case MS_LEER:
                atender_lectura(socket_kernel_memory_operaciones, stick, logger, memory_delay);
                break;

            case MS_ESCRIBIR:
                atender_escritura(socket_kernel_memory_operaciones, stick, logger, memory_delay);
                break;

            default:
                log_error(logger, "Operacion desconocida");
                break;
        }
    }

    cerrar_conexion(socket_kernel_memory_operaciones, logger);
    free(stick->memoria);
    free(stick);
    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
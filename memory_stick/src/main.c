#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include <commons/config.h>
#include <commons/log.h>

#include "utils/conexion.h"
#include "memory_stick.h"

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
    char* puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

    t_log* logger = log_create("memory_stick.log", "MEMORY_STICK", 1, LOG_LEVEL_INFO);

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

    int servidor = iniciar_servidor_modulo(logger, puerto_escucha, "Memory Stick");

    if(servidor == -1) {
        cerrar_conexion(socket_kernel_memory_operaciones, logger);
        free(stick->memoria);
        free(stick);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    while(1)
    {
        int cpu = esperar_cliente_modulo(logger, servidor, "Memory Stick");
        if(cpu == -1) continue;

        handshake_servidor(cpu, logger);
        int id_cpu;
        recibir_int(cpu, &id_cpu);
        log_info(logger, "## CPU %d Conectada", id_cpu);

        t_args_cpu* args = malloc(sizeof(t_args_cpu));
        args->socket = cpu;
        args->stick = stick;
        args->logger = logger;

        crear_hilo(atender_cpu, args);
    }

    return EXIT_SUCCESS;
}
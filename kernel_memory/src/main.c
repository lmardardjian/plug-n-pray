#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/config.h>
#include <commons/log.h>
#include "utils/sockets.h"
#include <sys/socket.h>

// Inicia el servidor
int iniciar_servidor_modulo(t_log* logger, char* puerto, char* nombre_modulo) {
    log_info(logger, "Iniciando %s en puerto %s", nombre_modulo, puerto);

    int servidor = iniciar_servidor(puerto);
    if (servidor == -1) {
        log_error(logger, "Error al iniciar %s", nombre_modulo);
        return -1;
    }

    log_info(logger, "%s listo para recibir conexiones", nombre_modulo);
    return servidor;
}

// Espera un cliente
int esperar_cliente_modulo(t_log* logger, int servidor, char* nombre_modulo) {
    log_info(logger, "Esperando cliente en %s...", nombre_modulo);

    int cliente = esperar_cliente(servidor);
    if (cliente == -1) {
        log_error(logger, "Error al aceptar cliente en %s", nombre_modulo);
        return -1;
    }

    log_info(logger, "Cliente conectado a %s", nombre_modulo);
    return cliente;
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Falta archivo de config\n");
        return EXIT_FAILURE;
    }

    t_config* config = config_create(argv[1]);
    if (config == NULL) {
        printf("Error al leer config\n");
        return EXIT_FAILURE;
    }

    char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");

    t_log* logger = log_create("kernel_memory.log", "MEMORY", 1, LOG_LEVEL_INFO);

    int servidor = iniciar_servidor_modulo(logger, puerto, "Kernel Memory");
    if (servidor == -1) {
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    int cliente = esperar_cliente_modulo(logger, servidor, "Kernel Memory");
    if (cliente == -1) {
        liberar_conexion(servidor);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    char buffer[100];

    int bytes = recv(cliente, buffer, sizeof(buffer), 0);
    if (bytes > 0) {
        log_info(logger, "Mensaje recibido");
        printf("Mensaje: %s\n", buffer);
    } else {
        log_error(logger, "Error al recibir mensaje");
    }

    liberar_conexion(cliente);
    liberar_conexion(servidor);

    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
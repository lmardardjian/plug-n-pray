#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/config.h>
#include <commons/log.h>
#include "utils/conexion.h"
#include <sys/socket.h>

int main(int argc, char* argv[]) {
    // si no hay argumento
    if (argc < 2) {
        printf("Falta archivo de config\n");
        return EXIT_FAILURE;
    }

    // me traigo el archivo de configuracion e intento leerlo
    t_config* config = config_create(argv[1]);
    if (config == NULL) {
        printf("Error al leer config\n");
        return EXIT_FAILURE;
    }

    // me traigo del archivo de configuracion la IP del kernel scheduler y el puerto en el que escucha
    char* ip_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    char* puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

    // creo el logger
    t_log* logger = log_create("memory_stick.log", "MEMORY_STICK", 1, LOG_LEVEL_INFO);

    // creo la conexion
    int conexion = conectar_a_modulo(logger, ip_memory, puerto_memory, "Kernel Memory");
    if (conexion != -1) {
        enviar_mensaje(conexion, "Hola Kernel Memory desde Stick", logger);
        cerrar_conexion(conexion, logger);
    }

    int servidor = iniciar_servidor_modulo(logger, puerto_escucha, "Memory Stick");
    if (servidor == -1) {
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    int cliente = esperar_cliente_modulo(logger, servidor, "Memory Stick");
    if (cliente == -1) {
        cerrar_conexion(servidor, logger);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    char buffer[100];

    recibir_mensaje(cliente, buffer, sizeof(buffer), logger);

    // libero recursos
    cerrar_conexion(cliente, logger);
    cerrar_conexion(servidor, logger);

    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
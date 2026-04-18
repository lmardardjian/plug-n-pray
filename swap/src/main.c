#include <stdio.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/log.h>
#include "utils/conexion.h"
#include <string.h>
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
    char* ip = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    // creo el logger
    t_log* logger = log_create("swap.log", "SWAP", 1, LOG_LEVEL_INFO);

    // creo la conexion
    int conexion = conectar_a_modulo(logger, ip, puerto, "Kernel_Memory");
    if (conexion == -1) {
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    enviar_mensaje(conexion, "Hola Kernel Memory desde Swap", logger);

    // libero recursos
    cerrar_conexion(conexion, logger);

    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
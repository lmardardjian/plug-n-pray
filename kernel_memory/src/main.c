#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/dictionary.h>
#include "utils/conexion.h"
#include "utils/constantes.h"
#include "utils/hilos.h"
#include <sys/socket.h>
#include <pthread.h>
#include "kernelmemory.h"

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


    t_dictionary* procesos = dictionary_create();
    while(1)
    {
        int cliente = esperar_cliente_modulo(logger, servidor, "Kernel Memory");
        if(cliente == -1)
        {
            log_error(logger, "Error aceptando cliente");
            continue;
        }

        t_args_cliente* args = malloc(sizeof(t_args_cliente));
        args->socket = cliente;
        args->logger = logger;
        args->procesos = procesos;
        crear_hilo(atender_cliente, args);
    }

    cerrar_conexion(servidor, logger);
    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
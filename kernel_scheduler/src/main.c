#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/config.h>
#include <commons/log.h>
#include "utils/conexion.h"
#include <sys/socket.h>

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

    t_log* logger = log_create("kernel_scheduler.log", "KERNEL", 1, LOG_LEVEL_INFO);

    int servidor = iniciar_servidor_modulo(logger, puerto, "Kernel Scheduler");
    if (servidor == -1) {
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    int cliente = esperar_cliente_modulo(logger, servidor, "Kernel Scheduler");
    if (cliente == -1) {
        cerrar_conexion(servidor, logger);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    int32_t resultado_handshake = handshake_servidor(cliente, logger);

    if(resultado_handshake == -1) {
        log_error(logger, "Handshake fallido");
        return EXIT_FAILURE;
    }
    log_info(logger, "Handshake realizado correctamente");

    while(1){
        //chequeamos que no llegue la notificación por parte del Kernel Memory de que se detectó 
        //corrupción en la memoria y por lo tanto un BSoD (podría ser un bool global?).
        if(!blue_screen_of_death) {
            
        }
        else {
            list_destroy_and_destroy_elements(p_activos_global, kill_all_processes);
            return BSOD;
        }

    }


    char buffer[100];

    recibir_mensaje(cliente, buffer, sizeof(buffer), logger);

    cerrar_conexion(cliente, logger);
    cerrar_conexion(servidor, logger);

    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
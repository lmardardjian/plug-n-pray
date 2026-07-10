// main.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <commons/config.h>
#include <commons/log.h>

#include "utils/conexion.h"
#include "utils/mensajes.h"

#include "cpu.h"

#include "utils/constantes.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Uso: ./cpu [config] [identificador]\n"); //raro este string
        return EXIT_FAILURE;
    }
    t_config* config = config_create(argv[1]);

    if (config == NULL) {
        printf("Error al leer config\n");
        return EXIT_FAILURE;
    }

    // LOGGER
    char nombre_log[MAX_NOMBRE_LOG];
    snprintf(nombre_log, sizeof(nombre_log), "cpu_%s.log", argv[2]);

    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    t_log* logger = log_create(nombre_log, "CPU", true, log_level_from_string(log_level_str));

    // SEGMENT_MAX_SIZE: necesario para la MMU (debe coincidir con el
    // configurado en el Kernel Memory)
    tam_max_segmento = (uint32_t)config_get_int_value(config, "SEGMENT_MAX_SIZE");

    // KERNEL SCHEDULER
    char* ip_scheduler = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char* puerto_scheduler = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");

    int fd_scheduler = conectar_a_modulo(logger, ip_scheduler, puerto_scheduler, "Kernel Scheduler");
    handshake_cliente(fd_scheduler, logger, MODULO_CPU);
    enviar_string(fd_scheduler, argv[2]);

    // KERNEL MEMORY
    char* ip_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    int fd_memory = conectar_a_modulo(logger, ip_memory, puerto_memory, "Kernel Memory");
    handshake_cliente(fd_memory, logger, MODULO_CPU);
    enviar_string(fd_memory, argv[2]);

    // LOOP PRINCIPAL
    while (1) {
       if (fd_scheduler < 0) {
            log_error(logger, "No se pudo conectar al Kernel Scheduler");
            return EXIT_FAILURE;
        }

        uint32_t pid;

        if (recibir_uint32(fd_scheduler, &pid) <= 0) {
            log_error(logger, "Kernel Scheduler desconectado");
            break;
        }

        pid_actual = (int)pid;
        log_info(logger, "## CPU recibió PID: %d", pid_actual);
        ciclo_instruccion(fd_scheduler, fd_memory, logger);
    }
    
    cerrar_conexion(fd_scheduler, logger);
    cerrar_conexion(fd_memory, logger);
    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
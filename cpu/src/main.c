#include <stdio.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/log.h>
#include "utils/conexion.h"
#include <string.h>
#include <sys/socket.h>
#include <stdint.h>
#include "ciclo.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Uso: ./cpu [config] [identificador]\n");
        return EXIT_FAILURE;
    }

    t_config* config = config_create(argv[1]);
    if (!config) { printf("Error config\n"); return EXIT_FAILURE; }

    // Logger con el identificador en el nombre del archivo
    char nombre_log[64];
    sprintf(nombre_log, "cpu_%s.log", argv[2]);
    t_log* logger = log_create(nombre_log, "CPU", 1, LOG_LEVEL_INFO);

    // Conexión al Kernel Scheduler
    char* ip_ks    = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char* puerto_ks = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");
    int fd_scheduler = conectar_a_modulo(logger, ip_ks, puerto_ks, "Kernel_Scheduler");
    handshake_cliente(fd_scheduler, logger, MODULO_CPU);

    // Conexión al Kernel Memory
    char* ip_km    = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_km = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    int fd_memory = conectar_a_modulo(logger, ip_km, puerto_km, "Kernel_Memory");
    handshake_cliente(fd_memory, logger, MODULO_CPU);

    // Loop principal: esperar PID y ejecutar
    while (1) {
        // El Scheduler nos manda el PID a ejecutar
        uint32_t pid;
        int r = recibir_uint32(fd_scheduler, &pid);
        if (r <= 0) break; // scheduler cerró conexión

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


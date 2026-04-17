#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/config.h>
#include <commons/log.h>
#include "utils/sockets.h"
#include <sys/socket.h>

//Cliente
int conectar_a_modulo(t_log* logger, char* ip, char* puerto, char* nombre_modulo) {
    log_info(logger, "Conectando a %s (%s:%s)", nombre_modulo, ip, puerto);

    int conexion = crear_conexion(ip, puerto);
    if (conexion == -1) {
        log_error(logger, "No se pudo conectar a %s", nombre_modulo);
        return -1;
    }

    log_info(logger, "Conectado correctamente a %s", nombre_modulo);
    return conexion;
}

void enviar_mensaje(int conexion, char* mensaje, t_log* logger) {
    send(conexion, mensaje, strlen(mensaje) + 1, 0);
    log_info(logger, "Mensaje enviado");
}

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
        liberar_conexion(conexion);
    }

    int servidor = iniciar_servidor_modulo(logger, puerto_escucha, "Memory Stick");
    if (servidor == -1) {
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    int cliente = esperar_cliente_modulo(logger, servidor, "Memory Stick");
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
        printf("Stick recibio: %s\n", buffer);
    } else {
        log_error(logger, "Error al recibir mensaje");
    }

    // libero recursos
    liberar_conexion(cliente);
    liberar_conexion(servidor);

    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
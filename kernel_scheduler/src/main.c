#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <commons/config.h>
#include <commons/log.h>
#include "utils/conexion.h"
#include <sys/socket.h>
#include "scheduler.h"

//Antes las variables estaban dentro del main. Las movemos acá para hacerlas globales.
t_log* logger;
t_config* config;
int servidor;
bool blue_screen_of_death = false;

//Funcion que ejecuta cada cliente conectado
void* atender_cliente_especifico(void* arg) {
    int cliente = *(int*)arg;
    free(arg);

    int32_t resultado_handshake = handshake_servidor(cliente, logger);
    if(resultado_handshake == -1) {
        log_error(logger, "Handshake fallido con cliente %d", cliente);
        cerrar_conexion(cliente, logger);
        return NULL;
    }
    log_info(logger, "Handshake realizado correctamente con cliente %d", cliente);

    char buffer[100];
    while(1) {
        //cambiar recibir_mensaje por nuestra función de deserializacion
        int bytes_recibidos = recibir_mensaje(cliente, buffer, sizeof(buffer), logger);
        if (bytes_recibidos <= 0) {
            log_error(logger, "EL cliente (%d) se desconecto", cliente);
            break;
        }
        // Dependiendo del mensaje (ej: llegó un proceso, 
        // terminó un IO), harías un agregar_a_ready(pcb)
        // o lo que corresponda.
    }

    cerrar_conexion(cliente, logger);
    return NULL;
}

//HILO SERVIDOR
void* escuchar_conexiones(void* arg) {
    while(1) {
        log_info(logger, "ESperando nueva conexión de un cliente...");
        int cliente = esperar_cliente_modulo(logger, servidor, "Kernel Scheduler");
    
        if (cliente != -1) {
            log_info(logger, "Nuevo cliente conectado! Socket: %d", cliente);

            pthread_t hilo_cliente;

            int* socket_cliente = malloc(sizeof(int));
            *socket_cliente = cliente;

            pthread_create(&hilo_cliente, NULL, atender_cliente_especifico, (void*)socket_cliente); 
            pthread_detach(hilo_cliente);
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Falta archivo de config\n");
        return EXIT_FAILURE;
    }

    config = config_create(argv[1]);
    if (config == NULL) {
        printf("Error al leer config\n");
        return EXIT_FAILURE;
    }

    char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");

    logger = log_create("kernel_scheduler.log", "KERNEL", 1, LOG_LEVEL_INFO);

    servidor = iniciar_servidor_modulo(logger, puerto, "Kernel Scheduler");
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
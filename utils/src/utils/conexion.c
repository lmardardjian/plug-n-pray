#include <utils/conexion.h>
#include <utils/sockets.h>
#include <stdint.h>
#include <sys/socket.h>
#include <string.h>

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

void cerrar_conexion(int socket, t_log* logger) {
    liberar_conexion(socket);
    log_info(logger, "Conexion liberada correctamente");
}

void enviar_mensaje(int conexion, char* mensaje, t_log* logger) {
    send(conexion, mensaje, strlen(mensaje) + 1, 0);
    log_info(logger, "Mensaje enviado");
}

int recibir_mensaje(int conexion, char* buffer, int size, t_log* logger) {
    int bytes = recv(conexion, buffer, size, 0);
    
    if (bytes > 0) {
        buffer[bytes] = '\0'; // importante
        log_info(logger, "Mensaje recibido");
    } else {
        log_error(logger, "Error al recibir mensaje");
    }

    return bytes;
}

int handshake_cliente(int socket_conexion, t_log* logger, int32_t id_modulo)
{
    int32_t handshake = 1;
    int32_t resultado;

    // manda el handshake
    send(socket_conexion, &handshake, sizeof(int32_t), 0);

    // espera respuesta
    // MSG_WAITALL es para que espere el mensaje completo para devolverlo
    recv(socket_conexion, &resultado, sizeof(int32_t), MSG_WAITALL);

    if (resultado != 0) {
        log_error(logger, "Handshake rechazado");
        return -1;
    }

    // manda el id del cliente para que el servidor sepa quien lo consume
    send(socket_conexion, &id_modulo, sizeof(int32_t), 0);

    log_info(logger, "Handshake OK. Envié mi ID: %d", id_modulo);

    return 0;
}

int32_t handshake_servidor(int socket_conexion, t_log* logger)
{
    int32_t handshake;
    int32_t resultado_ok = 0;
    int32_t resultado_error = -1;
    int32_t id_cliente;

    // recibe handshake
    // MSG_WAITALL es para que espere el mensaje completo para devolverlo
    recv(socket_conexion, &handshake, sizeof(int32_t), MSG_WAITALL);

    if (handshake != 1) {
        send(socket_conexion, &resultado_error, sizeof(int32_t), 0);
        log_error(logger, "Handshake inválido");
        return -1;
    }

    // responde OK
    send(socket_conexion, &resultado_ok, sizeof(int32_t), 0);

    // recibe el id del cliente que lo consume para devolverlo
    recv(socket_conexion, &id_cliente, sizeof(int32_t), MSG_WAITALL);

    log_info(logger, "Handshake OK. Cliente conectado con ID: %d", id_cliente);

    return id_cliente;
}


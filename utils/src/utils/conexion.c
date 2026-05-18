#include <utils/conexion.h>
#include <utils/sockets.h>
#include <stdint.h>
#include <sys/socket.h>
#include <string.h>

int iniciar_servidor_modulo(t_log* logger, char* puerto, char* nombre_modulo) {
    log_info(logger, "FN_INICIAR_SRV: Iniciando %s en puerto %s", nombre_modulo, puerto);

    int servidor = iniciar_servidor(puerto);
    if (servidor == -1) {
        log_error(logger, "FN_INICIAR_SRV: Error al iniciar %s", nombre_modulo);
        return -1;
    }

    log_info(logger, "FN_INICIAR_SRV: %s listo para recibir conexiones", nombre_modulo);
    return servidor;
}

int esperar_cliente_modulo(t_log* logger, int servidor, char* nombre_modulo) {
    log_info(logger, "FN_ESPERAR_CNX: Esperando cliente en %s...", nombre_modulo);

    int cliente = esperar_cliente(servidor);
    if (cliente == -1) {
        log_error(logger, "FN_ESPERAR_CNX: Error al aceptar cliente en %s", nombre_modulo);
        return -1;
    }

    log_info(logger, "FN_ESPERAR_CNX: Cliente conectado a %s", nombre_modulo);
    return cliente;
}

int conectar_a_modulo(t_log* logger, char* ip, char* puerto, char* nombre_modulo) {
    log_info(logger, "FN_CONECT_CNX: Conectando a %s (%s:%s)", nombre_modulo, ip, puerto);

    int conexion = crear_conexion(ip, puerto);
    if (conexion == -1) {
        log_error(logger, "FN_CONECT_CNX: No se pudo conectar a %s", nombre_modulo);
        return -1;
    }

    log_info(logger, "FN_CONECT_CNX: Conectado correctamente a %s", nombre_modulo);
    return conexion;
}

void cerrar_conexion(int socket, t_log* logger) {
    liberar_conexion(socket);
    log_info(logger, "FN_CERRAR_CNX: Conexion liberada correctamente");
}

void enviar_mensaje(int conexion, char* mensaje, t_log* logger) {
    send(conexion, mensaje, strlen(mensaje) + 1, 0);
    log_info(logger, "FN_ENVIAR_MSG: Mensaje enviado");
}

int recibir_mensaje(int conexion, char* buffer, int size, t_log* logger) {
    int bytes = recv(conexion, buffer, size, 0);
    
    if (bytes > 0 && bytes < size) {
        buffer[bytes] = '\0';
        log_info(logger, "FN_RECIBIR_MSG: Mensaje recibido");
    } 
    else if (bytes > 0 && bytes == size) {
        buffer[size - 1] = '\0';
        log_info(logger, "FN_RECIBIR_MSG: Mensaje recibido");
    }
    else
    {
        log_error(logger, "FN_RECIBIR_MSG: Error al recibir mensaje");
    }
    

    return bytes;
}

void enviar_int(int conexion, int valor)
{
    send(conexion, &valor, sizeof(int), 0);
}

int recibir_int(int conexion, int* valor)
{
    return recv(conexion, valor, sizeof(int), MSG_WAITALL);
}

int handshake_cliente(int socket_conexion, t_log* logger, int32_t id_modulo)
{
    int32_t handshake = 1;
    int32_t resultado;

    log_info(logger, "HANDSHAKE: Enviando handshake...");

    int enviado = send(socket_conexion, &handshake, sizeof(int32_t), 0);

    if (enviado <= 0) {
        log_error(logger, "HANDSHAKE: Error enviando handshake");
        return -1;
    }

    log_info(logger, "HANDSHAKE: Esperando respuesta handshake...");

    int recibido = recv(socket_conexion, &resultado, sizeof(int32_t), MSG_WAITALL);

    if (recibido <= 0) {
        log_error(logger, "HANDSHAKE: Error recibiendo respuesta handshake");
        return -1;
    }
    else if (resultado != 0) {
        log_error(logger, "HANDSHAKE: Handshake rechazado");
        return -1;
    }

    log_info(logger, "HANDSHAKE: Handshake aceptado");

    enviado = send(socket_conexion, &id_modulo, sizeof(int32_t), 0);

    if (enviado <= 0) {
        log_error(logger, "HANDSHAKE: Error enviando ID del modulo");
        return -1;
    }

    log_info(logger, "HANDSHAKE: ID enviado correctamente: %d", id_modulo);
    return 0;
}

int32_t handshake_servidor(int socket_conexion, t_log* logger)
{
    int32_t handshake;
    int32_t resultado_ok = 0;
    int32_t resultado_error = -1;
    int32_t id_cliente;

    log_info(logger, "HANDSHAKE: Esperando handshake...");

    int recibido = recv(socket_conexion, &handshake, sizeof(int32_t), MSG_WAITALL);

    if (recibido <= 0) {
        log_error(logger, "HANDSHAKE: Error recibiendo handshake");
        return -1;
    }

    if (handshake != 1) {
        log_error(logger, "HANDSHAKE: Handshake invalido");
        send(socket_conexion, &resultado_error, sizeof(int32_t), 0);
        return -1;
    }

    log_info(logger, "HANDSHAKE: Handshake valido");

    int enviado = send(socket_conexion, &resultado_ok, sizeof(int32_t), 0);

    if (enviado <= 0) {
        log_error(logger, "HANDSHAKE: Error enviando ACK handshake");
        return -1;
    }

    recibido = recv(socket_conexion, &id_cliente, sizeof(int32_t), MSG_WAITALL);

    if (recibido <= 0) {
        log_error(logger, "HANDSHAKE: Error recibiendo ID del cliente");
        return -1;
    }

    log_info(logger, "HANDSHAKE: Cliente conectado correctamente. ID modulo: %d", id_cliente);

    return id_cliente;
}

int enviar_opcode(int socket, op_code codigo)
{
    return send(socket, &codigo, sizeof(op_code), 0);
}

int recibir_opcode(int socket, op_code* codigo)
{
    return recv(socket, codigo, sizeof(op_code), MSG_WAITALL);
}
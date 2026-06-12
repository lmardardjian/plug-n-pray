#include "sockets.h"
#include "conexion.h"
#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

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
        return cliente;
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

void enviar_uint32(int conexion, uint32_t valor)
{
    send(conexion, &valor, sizeof(uint32_t), 0);
}

int recibir_uint32(int conexion, uint32_t* valor)
{
    return recv(conexion, valor, sizeof(uint32_t), MSG_WAITALL);
}

void enviar_uint8(int conexion, uint8_t valor)
{
    send(conexion, &valor, sizeof(uint8_t), 0);
}

int recibir_uint8(int conexion, uint8_t* valor)
{
    return recv(conexion, valor, sizeof(uint8_t), MSG_WAITALL);
}

void enviar_string(int conexion, char* string)
{
    uint32_t length = strlen(string) + 1;
    enviar_uint32(conexion, length);
    send(conexion, string, length, 0);
}

int recibir_string(int conexion, char* buffer, int max_size)
{
    uint32_t length;
    recibir_uint32(conexion, &length);
    
    if(length > max_size) 
        return -1;

    return recv(conexion, buffer, length, MSG_WAITALL);
}

void enviar_contexto_serializado(int conexion, t_contexto* contexto)
{
    //send(conexion, contexto, sizeof(t_contexto), 0);
    enviar_uint32(conexion, contexto->pc);
    enviar_uint8(conexion, contexto->ax);
    enviar_uint8(conexion, contexto->bx);
    enviar_uint8(conexion, contexto->cx);
    enviar_uint8(conexion, contexto->dx);
    enviar_uint32(conexion, contexto->eax);
    enviar_uint32(conexion, contexto->ebx);
    enviar_uint32(conexion, contexto->ecx);
    enviar_uint32(conexion, contexto->edx);
    enviar_uint32(conexion, contexto->si);
    enviar_uint32(conexion, contexto->di);

    // tabla de segmentos
    uint32_t cantidad = list_size(contexto->tabla_segmentos);
    enviar_uint32(conexion, cantidad);

    for (int i = 0; i < cantidad; i++) {
        t_segmento* seg = list_get(contexto->tabla_segmentos, i);
        enviar_uint32(conexion, seg->id_segmento);
        enviar_uint32(conexion, seg->base);
        enviar_uint32(conexion, seg->limite);
    }
}

void recibir_contexto_serializado(int conexion, t_contexto* contexto)
{
    //recv(conexion, contexto, sizeof(t_contexto), MSG_WAITALL);
    recibir_uint32(conexion, &contexto->pc);
    recibir_uint8(conexion, &contexto->ax);
    recibir_uint8(conexion, &contexto->bx);
    recibir_uint8(conexion, &contexto->cx);
    recibir_uint8(conexion, &contexto->dx);
    recibir_uint32(conexion, &contexto->eax);
    recibir_uint32(conexion, &contexto->ebx);
    recibir_uint32(conexion, &contexto->ecx);
    recibir_uint32(conexion, &contexto->edx);
    recibir_uint32(conexion, &contexto->si);
    recibir_uint32(conexion, &contexto->di);

    uint32_t cantidad;
    recibir_uint32(conexion, &cantidad);

    contexto->tabla_segmentos = list_create();
    for (int i = 0; i < cantidad; i++) {
        t_segmento* seg = malloc(sizeof(t_segmento));
        recibir_uint32(conexion, &seg->id_segmento);
        recibir_uint32(conexion, &seg->base);
        recibir_uint32(conexion, &seg->limite);
        list_add(contexto->tabla_segmentos, seg);
    }
}

void destruir_tabla_segmentos(t_list* tabla)
{
    list_destroy_and_destroy_elements(tabla, free);
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

void enviar_contexto_completo(int conexion, t_contexto* contexto)
{
    enviar_uint32(conexion, contexto->pc);
    send(conexion, &contexto->ax,  sizeof(uint8_t),  0);
    send(conexion, &contexto->bx,  sizeof(uint8_t),  0);
    send(conexion, &contexto->cx,  sizeof(uint8_t),  0);
    send(conexion, &contexto->dx,  sizeof(uint8_t),  0);
    enviar_uint32(conexion, contexto->eax);
    enviar_uint32(conexion, contexto->ebx);
    enviar_uint32(conexion, contexto->ecx);
    enviar_uint32(conexion, contexto->edx);
    enviar_uint32(conexion, contexto->si);
    enviar_uint32(conexion, contexto->di);

    uint32_t cantidad = (uint32_t)list_size(contexto->tabla_segmentos);
    enviar_uint32(conexion, cantidad);
    for (uint32_t i = 0; i < cantidad; i++) {
        t_segmento* s = list_get(contexto->tabla_segmentos, i);
        enviar_uint32(conexion, s->id_segmento);
        enviar_uint32(conexion, s->base);
        enviar_uint32(conexion, s->limite);
    }
}

void recibir_contexto_completo(int conexion, t_contexto* contexto)
{
    recibir_uint32(conexion, &contexto->pc);
    recv(conexion, &contexto->ax,  sizeof(uint8_t),  MSG_WAITALL);
    recv(conexion, &contexto->bx,  sizeof(uint8_t),  MSG_WAITALL);
    recv(conexion, &contexto->cx,  sizeof(uint8_t),  MSG_WAITALL);
    recv(conexion, &contexto->dx,  sizeof(uint8_t),  MSG_WAITALL);
    recibir_uint32(conexion, &contexto->eax);
    recibir_uint32(conexion, &contexto->ebx);
    recibir_uint32(conexion, &contexto->ecx);
    recibir_uint32(conexion, &contexto->edx);
    recibir_uint32(conexion, &contexto->si);
    recibir_uint32(conexion, &contexto->di);

    uint32_t cantidad;
    recibir_uint32(conexion, &cantidad);

    for (uint32_t i = 0; i < cantidad; i++) {
        t_segmento* s = malloc(sizeof(t_segmento));
        recibir_uint32(conexion, &s->id_segmento);
        recibir_uint32(conexion, &s->base);
        recibir_uint32(conexion, &s->limite);
        list_add(contexto->tabla_segmentos, s);
    }
}

void enviar_int(int conexion, int valor)
{
    send(conexion, &valor, sizeof(int), 0);
}

int recibir_int(int conexion, int* valor)
{
    return recv(conexion, valor, sizeof(int), MSG_WAITALL);
}

void enviar_tipo_io(int conexion, tipo_io tipo)
{
    send(conexion, &tipo, sizeof(tipo_io), 0);
}

int recibir_tipo_io(int conexion, tipo_io* tipo)
{
    return recv(conexion, tipo, sizeof(tipo_io), MSG_WAITALL);
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

int enviar_buffer(int conexion, void* buffer, uint32_t tamanio)
{
    return send(conexion, buffer, tamanio, 0);
}

int recibir_buffer(int conexion, void* buffer, uint32_t tamanio)
{
    return recv(conexion, buffer, tamanio, MSG_WAITALL);
}

char* tipo_io_to_string(tipo_io tipo)
{
    switch(tipo)
    {
        case TIPO_IO_SLEEP:
            return "SLEEP";

        case TIPO_IO_STDIN:
            return "STDIN";

        case TIPO_IO_STDOUT:
            return "STDOUT";

        default:
            return "DESCONOCIDO";
    }
}
char* instruccion_to_string(tipo_instruccion tipo) {
    switch(tipo) {
        case INST_NOOP:
            return "NOOP";
        case INST_SET:
            return "SET";
        case INST_SUM:
            return "SUM";
        case INST_SUB:
            return "SUB";
        case INST_JNZ:
            return "JNZ";
        case INST_MOV_IN:
            return "MOV_IN";
        case INST_MOV_OUT:
            return "MOV_OUT";
        case INST_COPY_MEM:
            return "COPY_MEM";
        case INST_MUTEX_CREATE:
            return "MUTEX_CREATE";
        case INST_MUTEX_LOCK:
            return "MUTEX_LOCK";
        case INST_MUTEX_UNLOCK:
            return "MUTEX_UNLOCK";
        case INST_MEM_ALLOC:
            return "MEM_ALLOC";
        case INST_MEM_FREE:
            return "MEM_FREE";
        case INST_SLEEP:
            return "SLEEP";
        case INST_STDOUT:
            return "STDOUT";
        case INST_STDIN:
            return "STDIN";
        case INST_INIT_PROC:
            return "INIT_PROC";
        case INST_EXIT:
            return "EXIT";
        default:
            return "DESCONOCIDA";
    }
}
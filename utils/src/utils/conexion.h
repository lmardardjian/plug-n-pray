#ifndef UTILS_CONEXION_H
#define UTILS_CONEXION_H

#include "mensajes.h"
#include <commons/log.h>

int iniciar_servidor_modulo(t_log* logger, char* puerto, char* nombre_modulo);
int esperar_cliente_modulo(t_log* logger, int servidor, char* nombre_modulo);
int conectar_a_modulo(t_log* logger, char* ip, char* puerto, char* nombre_modulo);
void cerrar_conexion(int socket, t_log* logger);

// envios y recepciones de distintos mensajes
void enviar_mensaje(int conexion, char* mensaje, t_log* logger);
int recibir_mensaje(int conexion, char* buffer, int size, t_log* logger);
void enviar_int(int conexion, int valor);
int recibir_int(int conexion, int* valor);
void enviar_uint32(int conexion, uint32_t valor);
int recibir_uint32(int conexion, uint32_t* valor);
void enviar_string(int conexion, char* string);
int recibir_string(int conexion, char* buffer, int max_size);
int enviar_buffer(int conexion, void* buffer, uint32_t tamanio);
int recibir_buffer(int conexion, void* buffer, uint32_t tamanio);

// contextos
void enviar_contexto_serializado(int conexion, t_contexto* contexto);
void recibir_contexto_serializado(int conexion, t_contexto* contexto);

/// @brief devuelve el id del cliente con el que haga handshake
/// @param socket_conexion 
/// @return 
int32_t handshake_servidor(int socket_conexion, t_log* logger);
int handshake_cliente(int socket_conexion, t_log* logger, int32_t id_modulo);

// metodos para comunicacion de codigos de operacion
int enviar_opcode(int socket, op_code codigo);
int recibir_opcode(int socket, op_code* codigo);
char* tipo_io_to_string(tipo_io tipo);
char* instruccion_to_string(tipo_instruccion tipo);

#endif
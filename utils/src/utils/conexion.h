#ifndef UTILS_CONEXION_H
#define UTILS_CONEXION_H

#include <commons/log.h>

int iniciar_servidor_modulo(t_log* logger, char* puerto, char* nombre_modulo);
int esperar_cliente_modulo(t_log* logger, int servidor, char* nombre_modulo);
int conectar_a_modulo(t_log* logger, char* ip, char* puerto, char* nombre_modulo);
void cerrar_conexion(int socket, t_log* logger);
void enviar_mensaje(int conexion, char* mensaje, t_log* logger);
int recibir_mensaje(int conexion, char* buffer, int size, t_log* logger);

/// @brief devuelve el id del cliente con el que haga handshake
/// @param socket_conexion 
/// @return 
int32_t handshake_servidor(int socket_conexion, t_log* logger);
int handshake_cliente(int socket_conexion, t_log* logger, int32_t id_modulo);

#endif
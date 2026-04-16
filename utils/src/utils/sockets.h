#ifndef SOCKETS_H
#define SOCKETS_H

int crear_conexion(char* ip, char* puerto);
int iniciar_servidor(char* puerto);
int esperar_cliente(int socket_servidor);
void liberar_conexion(int socket);

#endif
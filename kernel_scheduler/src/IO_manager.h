#ifndef IO_MANAGER_H
#define IO_MANAGER_H

#include "pcb.h"
#include "utils/conexion.h"
#include "utils/constantes.h"

extern int socket_kernel_memory_operaciones;
extern pthread_mutex_t mutex_socket_km_operaciones;
typedef struct {
    uint32_t  pid;
    tipo_io tipo;
    uint32_t  sleep_ms;
    uint32_t  dir_logica;
    uint32_t  size;
    void*     datos;
} t_io_request;

typedef struct {
    char *nombre;
    tipo_io tipo;
    int socket_fd;
    t_log *logger;

    t_io_request req_en_vuelo;
    pthread_mutex_t mutex_req;
} t_io_interfaz;

void inicializar_io_manager();

void io_registrar_interfaz(const char* nombre, tipo_io tipo, int socket_fd, t_log* logger);
void manejar_syscall_io(t_pcb* proceso, t_io_request* req, t_log* logger);

#endif
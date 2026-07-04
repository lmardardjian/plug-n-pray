#ifndef IO_MANAGER_H
#define IO_MANAGER_H

#include "pcb.h"
#include "utils/conexion.h"
#include "utils/constantes.h"
#include <commons/collections/queue.h>
#include <semaphore.h>

extern int socket_kernel_memory_operaciones;
extern pthread_mutex_t mutex_socket_km_operaciones;
typedef struct {
    uint32_t pid;
    tipo_io tipo;
    uint32_t sleep_ms;
    uint32_t dir_fisica;
    uint32_t size;
    void* datos;
} t_io_request;

typedef struct {
    char *nombre;
    tipo_io tipo;
    int socket_fd;
    t_log *logger;

    t_queue* cola_requests;
    pthread_mutex_t mutex_cola;
    sem_t sem_requests;
} t_io_interfaz;

void inicializar_io_manager();

void io_registrar_interfaz(const char* nombre, tipo_io tipo, int socket_fd, t_log* logger);
void manejar_syscall_io(t_pcb* proceso, t_io_request* req, int socket_cpu, t_log* logger);

#endif
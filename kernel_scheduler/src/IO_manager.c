#include "procesos.h"
#include "scheduler.h"
#include "IO_manager.h"
#include <string.h>
#include <stdio.h>

// Socket hacia Kernel Memory (lo recibimos al registrar la primera interfaz)
extern int socket_kernel_memory;

// Lista de interfaces registradas
static t_list *s_interfaces = NULL; //interfaces sería los i/o?
static pthread_mutex_t s_mutex_interfaces = PTHREAD_MUTEX_INITIALIZER;

static t_io_interfaz* buscar_interfaz_por_tipo(tipo_io tipo) { //habría que usar la lista de i/o
    pthread_mutex_lock(&s_mutex_interfaces);
    t_io_interfaz* resultado = NULL;
    for (int i = 0; i < list_size(s_interfaces); i++) {
        t_io_interfaz* io = list_get(s_interfaces, i);
        if (io->tipo == tipo) {
            resultado = io;
            break;
        }
    }
    pthread_mutex_unlock(&s_mutex_interfaces);
    return resultado;
}

static char* armar_parametro(t_io_request* req) {
    char* param = malloc(20);
    switch (req->tipo) {
        case TIPO_IO_SLEEP:
            snprintf(param, 20, "%u", req->sleep_ms);
            break;
        case TIPO_IO_STDIN:
            snprintf(param, 20, "%u", req->size);
            break;
        case TIPO_IO_STDOUT:
            free(param);
            param = strdup(req->datos != NULL ? (char*)req->datos : "");
            break;
    }
    return param;
}

static void enviar_a_io(t_io_interfaz* io, t_io_request* req) {
    char* param = armar_parametro(req);

    enviar_opcode(io->socket_fd, IO_EJECUTAR);
    enviar_uint32(io->socket_fd, req->pid);
    enviar_string(io->socket_fd, param);

    free(param);
}

static char* leer_de_kernel_memory(uint32_t pid, uint32_t dir_logica, uint32_t size, t_log* logger) {
    enviar_opcode(socket_kernel_memory, KM_MEM_READ);

    op_code ack;
    if (recibir_opcode(socket_kernel_memory, &ack) <= 0) {
        log_error(logger, "Error al recibir ACK de KM en MEM_READ");
        return calloc(size + 1, 1);
    }

    return calloc(size + 1, 1);  // buffer vacío por ahora
    /*
    enviar_uint32(socket_kernel_memory, pid);                       ]
    enviar_uint32(socket_kernel_memory, dir_logica);                ]
    enviar_uint32(socket_kernel_memory, size);                      ]
                                                                    ]   //not in this checkpoint!
    char* buffer = malloc(size + 1);                                ]
    memset(buffer, 0, size + 1);                                    ]
    recibir_mensaje(socket_kernel_memory, buffer, size + 1, logger);]
    return buffer;                                                  ]
    */
}

static void escribir_en_kernel_memory(uint32_t pid, uint32_t dir_logica, char* datos, uint32_t size, t_log* logger) {
    enviar_opcode(socket_kernel_memory, KM_MEM_WRITE);

    op_code ack;
    if (recibir_opcode(socket_kernel_memory, &ack) <= 0) {
        log_error(logger, "Error al recibir ACK de KM en MEM_WRITE");
        return;
    }
    /*
    enviar_uint32(socket_kernel_memory, pid);                       ]
    enviar_uint32(socket_kernel_memory, dir_logica);                ]
    enviar_uint32(socket_kernel_memory, size);                      ]
                                                                    ]   //not in this checkpoint!
    enviar_mensaje(socket_kernel_memory, datos, logger);            ]
                                                                    ]
    op_code ack;                                                    ]
    recibir_opcode(socket_kernel_memory, &ack);                     ]
    */
}

static void* hilo_io_listener(void* arg) {
    t_io_interfaz* io = (t_io_interfaz*) arg;

    while (1) {
        pthread_mutex_lock(&io->mutex_req);
        t_io_request req_copia = io->req_en_vuelo;
        pthread_mutex_unlock(&io->mutex_req);

        uint32_t pid_finalizado = req_copia.pid;

        if (io->tipo == TIPO_IO_STDIN) {
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            recibir_mensaje(io->socket_fd, buffer, BUFFER_SIZE, io->logger);

            escribir_en_kernel_memory(pid_finalizado, req_copia.dir_logica, buffer, req_copia.size, io->logger);
        }

        op_code respuesta;
        if (recibir_opcode(io->socket_fd, &respuesta) <= 0) {
            log_error(io->logger, "IO %s (%s) desconectada", io->nombre, tipo_io_to_string(io->tipo));
            break;
        }

        if (respuesta == RESPUESTA_ERROR) {
            log_error(io->logger, "IO %s reportó error para PID %d", io->nombre, pid_finalizado);
            continue; //qué hacemos si da respuesta error en este lado?
        }

        log_info(io->logger, "## (%d) finalizó IO y pasa a READY / SUSP. READY", pid_finalizado);

        if (req_copia.datos != NULL) 
            free(req_copia.datos);

        t_pcb* proceso = quitar_de_block(pid_finalizado);
        if (proceso == NULL) {
            log_error(io->logger, "PID %d no estaba en BLOCK al finalizar IO", pid_finalizado);
            break;
        }

        if (proceso->estado == ESTADO_SUSP_BLOCK) {
            cambiar_estado(proceso, ESTADO_SUSP_READY, io->logger);
        } else {
            cambiar_estado(proceso, ESTADO_READY, io->logger);
            agregar_a_ready(proceso);
        }
    }

    return NULL;
}

void io_registrar_interfaz(const char* nombre, tipo_io tipo, int socket_fd, t_log* logger) {
    if (s_interfaces == NULL)
        s_interfaces = list_create();

    t_io_interfaz* io = malloc(sizeof(t_io_interfaz));
    io->nombre = strdup(nombre);
    io->tipo = tipo;
    io->socket_fd = socket_fd;
    io->logger = logger;
    memset(&io->req_en_vuelo, 0, sizeof(t_io_request));

    pthread_mutex_init(&io->mutex_req, NULL);
    pthread_mutex_lock(&s_mutex_interfaces);

    list_add(s_interfaces, io);
    pthread_mutex_unlock(&s_mutex_interfaces);

    crear_hilo(hilo_io_listener, io);

    log_info(logger, "## IO %s (%s) registrada y escuchando", nombre, tipo_io_to_string(tipo));
}

void manejar_syscall_io(t_pcb* proceso, t_io_request* req, t_log* logger) {

    log_info(logger, "## (%d) - Solicitó syscall: %s", proceso->pid, tipo_io_to_string(req->tipo));

    quitar_de_exec(proceso->pid);
    cambiar_estado(proceso, ESTADO_BLOCK, logger);
    agregar_a_block(proceso);

    t_io_interfaz* io = buscar_interfaz_por_tipo(req->tipo);
    if (io == NULL) {
        log_error(logger, "## (%d) No hay interfaz %s conectada. Proceso queda en BLOCK.", proceso->pid, tipo_io_to_string(req->tipo));
        return;
    }

    if (req->tipo == TIPO_IO_STDOUT) {
        req->datos = leer_de_kernel_memory(proceso->pid, req->dir_logica, req->size, logger);
    }

    pthread_mutex_lock(&io->mutex_req);
    io->req_en_vuelo = *req;
    pthread_mutex_unlock(&io->mutex_req);

    enviar_a_io(io, req);
}
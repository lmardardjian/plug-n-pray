#include "procesos.h"
#include "scheduler.h"
#include "IO_manager.h"
#include "utils/hilos.h"
#include <string.h>

//lista de interfaces registradas. Solo debe haber una de cada tipo.
static t_list *s_interfaces = NULL;
static pthread_mutex_t s_mutex_interfaces;

void inicializar_io_manager() {
    s_interfaces = list_create();
    pthread_mutex_init(&s_mutex_interfaces, NULL);
}

static t_io_interfaz* buscar_interfaz_por_tipo(tipo_io tipo) {

    pthread_mutex_lock(&s_mutex_interfaces);

    t_io_interfaz* resultado = NULL;
    int tamanio = list_size(s_interfaces);
    for (int i = 0; i < tamanio; i++) {
        t_io_interfaz* io = list_get(s_interfaces, i);
        if (io->tipo == tipo) {
            resultado = io;
            break;
        }
    }
    pthread_mutex_unlock(&s_mutex_interfaces);

    return resultado;
}

// ----------------------------- FUNCIONES AUXILIARES IO -----------------------------

static char* armar_parametro_io(t_io_request* req) {
    char* param = malloc(20); //mmm magic number

    //dependiendo el tipo de IO que lo esté pidiendo se crea el parámetro a enviar teniendo en cuenta lo que espera dicha intefaz.
    switch (req->tipo) {
        case TIPO_IO_SLEEP:
            snprintf(param, 20, "%u", req->sleep_ms);
            break;

        case TIPO_IO_STDIN:
            snprintf(param, 20, "%u", req->size);
            break;

        case TIPO_IO_STDOUT:
            free(param);
            //libero param porque el límite de 20 caracteres me es insuficiente para lo que necesito en este caso.
            param = strdup(req->datos != NULL ? (char*)req->datos : "");
            break;
    }
    return param;
}

static void enviar_a_io(t_io_interfaz* io, t_io_request* req) {
    char* param = armar_parametro_io(req);

    //envio la instrucción de hacer uso de una interfaz en concreto para un proceso en concreto junto con los parámetros requeridos. 
    enviar_opcode(io->socket_fd, IO_EJECUTAR);
    enviar_uint32(io->socket_fd, req->pid);
    enviar_string(io->socket_fd, param);

    free(param);
}

// ----------------------------- LECTURA/ESCRITURA IO -----------------------------

static char* leer_de_kernel_memory(uint32_t pid, uint32_t dir_logica, uint32_t size, t_log* logger) {

    pthread_mutex_lock(&mutex_socket_km_operaciones);

    enviar_opcode(socket_kernel_memory_operaciones, KM_MEM_READ);

    op_code ack;
    if (recibir_opcode(socket_kernel_memory_operaciones, &ack) <= 0) {
        log_error(logger, "Error al recibir ACK de KM en MEM_READ");

        pthread_mutex_unlock(&mutex_socket_km_operaciones);

        return calloc(size + 1, 1);
    }
    if(recibir_opcode(socket_kernel_memory_operaciones, &ack) == RESPUESTA_ERROR) {
        log_error(logger, "KM no pudo leer la memoria");
        return calloc(size + 1, 1);
    }
    
    enviar_uint32(socket_kernel_memory_operaciones, pid);
    enviar_uint32(socket_kernel_memory_operaciones, dir_logica);
    enviar_uint32(socket_kernel_memory_operaciones, size);
    
    char* buffer = malloc(size + 1);
    memset(buffer, 0, size + 1);
    recibir_buffer(socket_kernel_memory_operaciones, buffer, size);
    
    pthread_mutex_unlock(&mutex_socket_km_operaciones);

    return buffer;                                                  
}

static void escribir_en_kernel_memory(uint32_t pid, uint32_t dir_logica, char* datos, uint32_t size, t_log* logger) {

    pthread_mutex_lock(&mutex_socket_km_operaciones);

    enviar_opcode(socket_kernel_memory_operaciones, KM_MEM_WRITE);

    op_code ack;
    if (recibir_opcode(socket_kernel_memory_operaciones, &ack) <= 0) {
        log_error(logger, "Error al recibir ACK de KM en MEM_WRITE");

        pthread_mutex_unlock(&mutex_socket_km_operaciones);

        return;
    }
    if(recibir_opcode(socket_kernel_memory_operaciones, &ack) == RESPUESTA_ERROR) {
        log_error(logger, "KM no pudo escribir en la memoria");
        return;
    }
    
    enviar_uint32(socket_kernel_memory_operaciones, pid);
    enviar_uint32(socket_kernel_memory_operaciones, dir_logica);
    enviar_uint32(socket_kernel_memory_operaciones, size);
    
    enviar_buffer(socket_kernel_memory_operaciones, datos, size);

    recibir_opcode(socket_kernel_memory_operaciones, &ack);
    
   pthread_mutex_unlock(&mutex_socket_km_operaciones);

}

// ------------------------------------- LISTENER -------------------------------------

static void* hilo_io_listener(void* arg) {
    t_io_interfaz* io = (t_io_interfaz*) arg;

    while (1) {
        pthread_mutex_lock(&io->mutex_req);

        //obtengo la request en vuelo.
        t_io_request req_copia = io->req_en_vuelo;

        pthread_mutex_unlock(&io->mutex_req);

        //obtengo el pid del proceso.
        uint32_t pid_finalizado = req_copia.pid;

        //si el tipo del IO era STDIN, escribo en memoria lo solicitado.
        if (io->tipo == TIPO_IO_STDIN) {
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            recibir_string(io->socket_fd, buffer, BUFFER_SIZE);

            escribir_en_kernel_memory(pid_finalizado, req_copia.dir_logica, buffer, req_copia.size, io->logger);
        }

        //espero confirmación de que se ejecutó la acción asociada al tipo de IO.
        op_code respuesta;
        if (recibir_opcode(io->socket_fd, &respuesta) <= 0) {
            log_error(io->logger, "IO %s (%s) desconectada", io->nombre, tipo_io_to_string(io->tipo));
            break;
        }

        if (respuesta == RESPUESTA_ERROR) {
            log_error(io->logger, "IO %s reportó error para PID %d", io->nombre, pid_finalizado);
            break;
        }

        log_info(io->logger, "## (%d) finalizó IO y pasa a READY / SUSP. READY", pid_finalizado);

        if (req_copia.datos != NULL) // DUDA: Por qué if? Se podría llegar a liberar en otro lado?
            free(req_copia.datos);

        //trato de quitar el proceso de la lista de bloqueados.
        t_pcb* proceso = quitar_de_block(pid_finalizado);

        //si estaba ahí, lo muevo a la lista READY.
        if (proceso != NULL) {
            cambiar_estado(proceso, ESTADO_READY, io->logger);
            agregar_a_ready(proceso);
        } else {
            //no estaba en la lista de BLOCK porque fué suspendido. Lo busco en la lista de SUSP. BLOCK.
            proceso = quitar_de_susp_block_por_pid(pid_finalizado);
            if (proceso != NULL) {
                //si lo encuentro en la lista de SUSP. BLOCK lo muevo a SUSP. READY.
                cambiar_estado(proceso, ESTADO_SUSP_READY, io->logger);
                agregar_a_susp_ready(proceso);
            } else {
                log_error(io->logger, "PID %d no estaba en BLOCK ni SUSP_BLOCK", pid_finalizado);
            }
        }
    }
    return NULL;
}

// ----------------------------- REGISTRO INTERFACES IO  -----------------------------

void io_registrar_interfaz(const char* nombre, tipo_io tipo, int socket_fd, t_log* logger) {
    //busco si ya hay una interfaz registrada con este tipo.
    if(buscar_interfaz_por_tipo(tipo) == NULL) {
        //creo y hago uso de una estructura io interfaz.
        t_io_interfaz* io = malloc(sizeof(t_io_interfaz));
        io->nombre = strdup(nombre);
        io->tipo = tipo;
        io->socket_fd = socket_fd;
        io->logger = logger;
        memset(&io->req_en_vuelo, 0, sizeof(t_io_request));

        pthread_mutex_init(&io->mutex_req, NULL);

        pthread_mutex_lock(&s_mutex_interfaces);

        //la agrego a la lista de interfaces.
        list_add(s_interfaces, io);

        pthread_mutex_unlock(&s_mutex_interfaces);

        //creo un hilo para escuchar pedidos de ese tipo de interfaz.
        crear_hilo(hilo_io_listener, io);

        log_info(logger, "## IO %s (%s) registrada y escuchando", nombre, tipo_io_to_string(tipo));
    } else {
        log_error(logger, "IO %s de tipo %s ya está registrada", nombre, tipo_io_to_string(tipo));
    }
}

// ----------------------------- MANEJADOR DE SYSCALL IO -----------------------------

void manejar_syscall_io(t_pcb* proceso, t_io_request* req, int socket_cpu, t_log* logger) {

    log_info(logger, "## (%d) - Solicitó syscall: %s", proceso->pid, tipo_io_to_string(req->tipo));

    //bloqueo el proceso que recibo por parámetro.
    quitar_de_exec(proceso->pid);
    cambiar_estado(proceso, ESTADO_BLOCK, logger);
    agregar_a_block(proceso);

    //libero la cpu en la que estaba ejecutando el proceso.
    agregar_cpu_libre(socket_cpu);

    //busco la interfaz con la que debo trabajar.
    t_io_interfaz* io = buscar_interfaz_por_tipo(req->tipo);
    if (io == NULL) {
        log_error(logger, "## (%d) No hay interfaz %s conectada. Proceso queda en BLOCK.", proceso->pid, tipo_io_to_string(req->tipo));
        return;
    }

    //si el tipo de interfaz es STDOUT leo desde una dirección de memoria un tamaño de bytes.
    if (req->tipo == TIPO_IO_STDOUT) 
        req->datos = leer_de_kernel_memory(proceso->pid, req->dir_logica, req->size, logger);

    pthread_mutex_lock(&io->mutex_req);

    io->req_en_vuelo = *req; // DUDA: Por qué se hace esto?
    
    pthread_mutex_unlock(&io->mutex_req);

    enviar_a_io(io, req);
}
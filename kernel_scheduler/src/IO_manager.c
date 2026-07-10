#include "procesos.h"
#include "scheduler.h"
#include "IO_manager.h"
#include "utils/hilos.h"
#include <unistd.h>
#include <string.h>

//lista de interfaces registradas. Solo debe haber una de cada tipo.
static t_list *s_interfaces = NULL;
static pthread_mutex_t s_mutex_interfaces;

void inicializar_io_manager() {
    s_interfaces = list_create();
    pthread_mutex_init(&s_mutex_interfaces, NULL);
}

// ----------------------------- FUNCIONES AUXILIARES IO -----------------------------

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

static char* armar_parametro_io(t_io_request* req, uint32_t* out_tamanio) {
    char* param = malloc(MAX_PARAM_IO_LEN);

    //dependiendo el tipo de IO que lo esté pidiendo se crea el parámetro a enviar teniendo en cuenta lo que espera dicha intefaz.
    switch (req->tipo) {
        case TIPO_IO_SLEEP:
            snprintf(param, MAX_PARAM_IO_LEN, "%u", req->sleep_ms);
            *out_tamanio = strlen(param) + 1;
            break;

        case TIPO_IO_STDIN:
            snprintf(param, MAX_PARAM_IO_LEN, "%u", req->size);
            *out_tamanio = strlen(param) + 1;
            break;

        case TIPO_IO_STDOUT:
            free(param);
            //libero param porque el límite de MAX_PARAM_IO_LEN caracteres me puede ser insuficiente para lo que necesito en este caso.
            uint32_t tamanio = req->size;
            param = malloc(tamanio > 0 ? tamanio : 1);

            if (tamanio > 0 && req->datos != NULL)
                memcpy(param, req->datos, tamanio);

            *out_tamanio = tamanio;
            break;
    }
    return param;
}

static void enviar_a_io(t_io_interfaz* io, t_io_request* req) {
    uint32_t tamanio;
    void* param = armar_parametro_io(req, &tamanio);

    //envio la instrucción de hacer uso de una interfaz en concreto para un proceso en concreto junto con los parámetros requeridos. 
    enviar_opcode(io->socket_fd, IO_EJECUTAR);
    enviar_uint32(io->socket_fd, req->pid);

    enviar_uint32(io->socket_fd, tamanio);
    if (tamanio > 0)
        enviar_buffer(io->socket_fd, param, tamanio);

    free(param);
}

// ----------------------------- LECTURA/ESCRITURA IO -----------------------------

static char* leer_de_kernel_memory(uint32_t pid, uint32_t dir_fisica, uint32_t size, t_log* logger) {

    pthread_mutex_lock(&mutex_socket_km_operaciones);

    enviar_opcode(socket_kernel_memory_operaciones, KM_MEM_READ);
    enviar_uint32(socket_kernel_memory_operaciones, pid);
    enviar_uint32(socket_kernel_memory_operaciones, dir_fisica);
    enviar_uint32(socket_kernel_memory_operaciones, size);

    op_code ack;
    if (recibir_opcode(socket_kernel_memory_operaciones, &ack) <= 0 || ack == RESPUESTA_ERROR) {
        log_error(logger, "Error al leer memoria del KM");

        pthread_mutex_unlock(&mutex_socket_km_operaciones);

        return calloc(size + 1, 1);
    }
    
    char* buffer = malloc(size + 1);
    memset(buffer, 0, size + 1);
    recibir_buffer(socket_kernel_memory_operaciones, buffer, size);
    
    pthread_mutex_unlock(&mutex_socket_km_operaciones);

    return buffer;                                                  
}

static void escribir_en_kernel_memory(uint32_t pid, uint32_t dir_fisica, char* datos, uint32_t size, t_log* logger) {

    pthread_mutex_lock(&mutex_socket_km_operaciones);

    enviar_opcode(socket_kernel_memory_operaciones, KM_MEM_WRITE);
    enviar_uint32(socket_kernel_memory_operaciones, pid);
    enviar_uint32(socket_kernel_memory_operaciones, dir_fisica);
    enviar_uint32(socket_kernel_memory_operaciones, size);
    enviar_buffer(socket_kernel_memory_operaciones, datos, size);

    op_code ack;
    if (recibir_opcode(socket_kernel_memory_operaciones, &ack) <= 0 || ack == RESPUESTA_ERROR) {
        log_error(logger, "Error al recibir ACK de KM en MEM_WRITE");

        pthread_mutex_unlock(&mutex_socket_km_operaciones);

        return;
    }
   pthread_mutex_unlock(&mutex_socket_km_operaciones);
}

// ------------------------------------- LISTENER -------------------------------------

static void* hilo_io_listener(void* arg) {
    t_io_interfaz* io = (t_io_interfaz*) arg;

    while (1) {
        sem_wait(&io->sem_requests);

        pthread_mutex_lock(&io->mutex_cola);

        t_io_request* req = queue_pop(io->cola_requests);

        pthread_mutex_unlock(&io->mutex_cola);

        if (req == NULL) {
            log_error(io->logger, "## IO %s: sem indicó request pero la cola estaba vacía.", io->nombre);
            continue;
        }

        uint32_t pid_finalizado = req->pid;

        //si el tipo del IO era STDIN, escribo en memoria lo solicitado.
        if (io->tipo == TIPO_IO_STDIN) {
            char* buffer = calloc(req->size + 1, 1);
            if (req->size > 0 && recibir_buffer(io->socket_fd, buffer, req->size) <= 0) {
                log_error(io->logger, "## IO %s desconectada leyendo STDIN de PID %u", io->nombre, pid_finalizado);
                free(buffer);

                if (req->datos != NULL)
                    free(req->datos);

                free(req);
                break;
            }
            escribir_en_kernel_memory(pid_finalizado, req->dir_fisica, buffer, req->size, io->logger);
            free(buffer);
        }

        //espero confirmación de que se ejecutó la acción asociada al tipo de IO.
        op_code respuesta;
        if (recibir_opcode(io->socket_fd, &respuesta) <= 0) {
            log_error(io->logger, "IO %s (%s) desconectada", io->nombre, tipo_io_to_string(io->tipo));
            
            //libero el campo datos de req solo cuando venimos del caos STDOUT.
            if (req->datos != NULL)
                free(req->datos);
            
            free(req);  
            break;
        }

        if (respuesta == RESPUESTA_ERROR) {
            log_error(io->logger, "IO %s reportó error para PID %d", io->nombre, pid_finalizado);
            
            //libero el campo datos de req solo cuando venimos del caos STDOUT.
            if (req->datos != NULL)
                free(req->datos);
            
            free(req);
            break;
        }

        log_info(io->logger, "## (%d) finalizó IO y pasa a READY / SUSP. READY", pid_finalizado);

        //libero el campo datos de req solo cuando venimos del caos STDOUT.
        if (req->datos != NULL)
            free(req->datos);

        free(req);

        //trato de quitar el proceso de la lista de bloqueados.
        pthread_mutex_lock(&mutex_transicion_block);

        t_pcb* proceso = quitar_de_block(pid_finalizado);

        //si estaba ahí, lo muevo a la lista READY.
        if (proceso != NULL) {
            //destruyo su timer tiempo_susp
            temporal_destroy(proceso->tiempo_susp);
            proceso->tiempo_susp = NULL;
            cambiar_estado(proceso, ESTADO_READY, io->logger);
            agregar_a_ready(proceso);
        } else {
            //no estaba en la lista de BLOCK porque fué suspendido. Lo busco en la lista de SUSP. BLOCK.
            proceso = quitar_de_susp_block_por_pid(pid_finalizado);

            //si lo encuentro en la lista de SUSP. BLOCK lo muevo a SUSP. READY.
            if (proceso != NULL) {
                cambiar_estado(proceso, ESTADO_SUSP_READY, io->logger);
                agregar_a_susp_ready(proceso);
            } else {
                log_error(io->logger, "PID %d no estaba en BLOCK ni SUSP_BLOCK", pid_finalizado);
            }
        }

        pthread_mutex_unlock(&mutex_transicion_block);
    }

    //raro sería pero por las dudas si se desconecta la interfaz:

    //dreno request pendientes en la cola para no filtrar memoria. Los procesos quedan en block y eventualmente van a susp_block, no se pierden.
    pthread_mutex_lock(&io->mutex_cola);

    while (!queue_is_empty(io->cola_requests)) {
        t_io_request* pendiente = queue_pop(io->cola_requests);
        log_warning(io->logger, "## IO %s desconectada con PID %u aún pendiente.", io->nombre, pendiente->pid);
        
        if (pendiente->datos != NULL)
            free(pendiente->datos);
        
        free(pendiente);
    }
    pthread_mutex_unlock(&io->mutex_cola);

    //remuevo la interfaz de la lista para permitir que una nueva del mismo tipo pueda registrarse.
    log_error(io->logger, "## IO %s (%s) desconectada. Removiendo del registro.", io->nombre, tipo_io_to_string(io->tipo));

    pthread_mutex_lock(&s_mutex_interfaces);
    
    for (int i = 0; i < list_size(s_interfaces); i++) {
        if (list_get(s_interfaces, i) == io) {
            list_remove(s_interfaces, i);
            break;
        }
    }
    pthread_mutex_unlock(&s_mutex_interfaces);

    close(io->socket_fd);
    free(io->nombre);
    queue_destroy(io->cola_requests);
    sem_destroy(&io->sem_requests);
    pthread_mutex_destroy(&io->mutex_cola);
    free(io);

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

        io->cola_requests = queue_create();
        pthread_mutex_init(&io->mutex_cola, NULL);
        sem_init(&io->sem_requests, 0, 0);

        //la agrego a la lista de interfaces.
        pthread_mutex_lock(&s_mutex_interfaces);

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
        req->datos = leer_de_kernel_memory(proceso->pid, req->dir_fisica, req->size, logger);

    //copio al heap para que la cola sea dueña de la request.
    t_io_request* req_heap = malloc(sizeof(t_io_request));
    *req_heap = *req;

    pthread_mutex_lock(&io->mutex_cola);

    queue_push(io->cola_requests, req_heap);
    enviar_a_io(io, req);

    pthread_mutex_unlock(&io->mutex_cola);

    //aviso al listener que hay una request esperando respuesta.
    sem_post(&io->sem_requests);
}
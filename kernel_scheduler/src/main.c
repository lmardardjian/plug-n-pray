#include "procesos.h"
#include "scheduler.h"
#include "IO_manager.h"
#include "utils/hilos.h"
#include "corto_plazo.h"
#include "mutex_manager.h"
#include <commons/config.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>

// -- Globales -----------------------------------------

t_log* logger;
t_config* config;

uint32_t proximo_pid = 1; // el 0 lo usa el proceso inicial, por eso inicializa en 1
pthread_mutex_t mutex_pid;        

bool blue_screen_of_death = false;

int socket_kernel_memory;
pthread_mutex_t mutex_socket_km;

t_list* p_activos_global;
pthread_mutex_t mutex_p_activos;

int cant_prioridades = 0;

char** queues_algoritmos = NULL;

int servidor; //por qué es global? no podría hacerse local del main y pasarse como param a escuchar_conexiones?

// -- Identificación de módulos conectados -------------

void* atender_cpu(void* arg) {
    int socket_cpu = *(int*)arg;
    free(arg);

    log_info(logger, "## CPU %d Conectada", socket_cpu);

    agregar_cpu_libre(socket_cpu);

    while (1) {
        op_code opcode;
        if (recibir_opcode(socket_cpu, &opcode) <= 0) {
            log_warning(logger, "CPU %d desconectada", socket_cpu);
            break;
        }

        uint32_t pid;
        recibir_uint32(socket_cpu, &pid);

        t_pcb* proceso = encontrar_proceso_global(pid);

        if (proceso == NULL) {
            log_error(logger, "Proceso %d no encontrado", pid);
            break;
        }

        switch (opcode) {
            case KS_TICK_PROGRESS_CONTINUE:
                manejar_tick_progress(socket_cpu, proceso);
                break;

            case KS_FIN_QUANTUM:
                manejar_fin_quantum(socket_cpu, proceso);
                break;

            case KS_SYSCALL_IO:
                manejar_syscall_io_cpu(socket_cpu, proceso);
                break;
            
            case KS_MUTEX_CREATE:
                manejar_mutex_create(socket_cpu, proceso);
                break;

            case KS_MUTEX_LOCK:
                manejar_mutex_lock(socket_cpu, proceso);
                break;

            case KS_MUTEX_UNLOCK:
                manejar_mutex_unlock(socket_cpu, proceso);
                break;
                
            case KS_EXIT:
                manejar_exit(socket_cpu, proceso);
                break;

            case KS_INIT_PROC: 
                manejar_iniciar_proceso(socket_cpu, socket_kernel_memory);
                break;

            default:
                log_warning(logger, "Opcode desconocido desde CPU %d", socket_cpu);
                break;
        }
    }
}

// -- Hilo servidor — acepta CPUs e IOs -------------------------

void* escuchar_conexiones(void* arg) {
    while (1) {
        int cliente = esperar_cliente_modulo(logger, servidor, "Kernel Scheduler");
        if (cliente == -1) {
            log_warning(logger, "No se pudo conectar de forma correcta al cliente. Salteando ciclo del bucle");
            continue;
        }

        int32_t id_modulo = handshake_servidor(cliente, logger); //falta log si el handshake falla

        int* socket = malloc(sizeof(int));
        *socket = cliente;

        switch (id_modulo) {
            case MODULO_CPU:
                crear_hilo(atender_cpu, socket);
                break;
            case MODULO_IO:
                tipo_io tipo;
                if(recibir_tipo_io(cliente, &tipo)<=0) {
                log_error(logger, "Error recibiendo tipo IO");
                free(socket);
                break;
                }

                io_registrar_interfaz("io", tipo, cliente, logger);
                free(socket);
                break;
            default:
                log_warning(logger, "Módulo desconocido: %d", id_modulo);
                free(socket);
                break;
        }
    }
    return NULL;
}

void* escuchar_kernel_memory(void* arg) {
    while (1) {
        op_code opcode;
        if (recibir_opcode(socket_kernel_memory, &opcode) <= 0) {
            log_error(logger, "Kernel Memory desconectado");
            blue_screen_of_death = true;
            break;
        }

        switch (opcode) {
            case KM_BSOD:
                log_error(logger, "## Kernel Memory reportó corrupción de memoria");
                blue_screen_of_death = true;
                break;
            default:
                log_warning(logger, "Opcode inesperado de KM: %d", opcode);
                break;
        }
    }
    return NULL;
}

//-- Main ---------------------------------------------------------------

int main(int argc, char* argv[]) {

    if (argc < 3) {
    printf("Uso: %s [config] [path_proceso_inicial]\n", argv[0]); // raro ese string
    return EXIT_FAILURE;
    }


    // Config
    config = config_create(argv[1]);
    if (config == NULL) {
        printf("Error al leer config\n");
        return EXIT_FAILURE;
    }


    // Logger
    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    t_log_level log_level = log_level_from_string(log_level_str);
    logger = log_create("kernel_scheduler.log", "KERNEL", 1, log_level);

    // Inicializar las estructuras
    pthread_mutex_init(&mutex_pid, NULL);
    pthread_mutex_init(&mutex_socket_km, NULL);
    pthread_mutex_init(&mutex_p_activos, NULL);
    p_activos_global = list_create();     //todo esto podría ser una función
    inicializar_planificador();
    inicializar_mutexes();
    inicializar_corto_plazo();            //no se podría poner lo de esta función en inicializar_planificador?

    // Conectar con Kernel Memory
    char* ip_km = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_km = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    socket_kernel_memory = conectar_a_modulo(logger, ip_km, puerto_km, "Kernel Memory");

    if (socket_kernel_memory == -1) {
        log_error(logger, "No se pudo conectar al Kernel Memory");
        return EXIT_FAILURE;
    }

    if (handshake_cliente(socket_kernel_memory, logger, MODULO_KERNEL_SCHEDULER) == -1) {
        log_error(logger, "Handshake con Kernel Memory fallido");
        return EXIT_FAILURE;
    }
    log_info(logger, "## Conectado a Kernel Memory");

    int socket_km_notificaciones = conectar_a_modulo(logger, ip_km, puerto_km, "Kernel Memory notificaciones");

    if (socket_km_notificaciones == -1) {
        log_error(logger, "No se pudo conectar al Notificador del Kernel Memory");
        return EXIT_FAILURE;
    }

    if (handshake_cliente(socket_km_notificaciones, logger, MODULO_KERNEL_SCHEDULER) == -1) {
        log_error(logger, "Handshake con Notificador del Kernel Memory fallido");
        return EXIT_FAILURE;
    }
    log_info(logger, "## Conectado al Notificador del Kernel Memory");

    // Iniciar servidor para CPUs e IOs
    char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    servidor = iniciar_servidor_modulo(logger, puerto, "Kernel Scheduler");
    if (servidor == -1) {
        return EXIT_FAILURE; //falta log de error
    }

    queues_algoritmos = config_get_array_value(config, "QUEUES_ALGORITHMS");
    while (queues_algoritmos[cant_prioridades] != NULL)
        cant_prioridades++;

    // Arrancar hilo dispatcher, hilo servidor e hilo notificador
    crear_hilo(hilo_dispatcher, NULL);
    crear_hilo(escuchar_conexiones, NULL);
    crear_hilo(escuchar_kernel_memory, NULL);

    // Crear proceso inicial PID 0
    char* path_proceso_inicial = argv[2];
    t_pcb* proceso_inicial = crear_pcb(0, 0);//mmm magic number PROCESO_INICIAL y PRIORIDAD_MAXIMA?

    pthread_mutex_lock(&mutex_p_activos);

    list_add(p_activos_global, proceso_inicial);

    pthread_mutex_unlock(&mutex_p_activos);

    log_info(logger, "## (0) Se crea el proceso - Estado: NEW"); // raro este string

    // Avisar al Kernel Memory que cree el proceso

    pthread_mutex_lock(&mutex_socket_km);

    enviar_opcode(socket_kernel_memory, KM_CREAR_PROCESO);
    enviar_uint32(socket_kernel_memory, 0);                     //mmm magic number PROCESO_INICIAL?
    enviar_string(socket_kernel_memory, path_proceso_inicial);

    op_code opcode;
    if (recibir_opcode(socket_kernel_memory, &opcode)<=0 || opcode != RESPUESTA_OK) {
        log_error(logger, "Fallo al crear el proceso inicial");
        return EXIT_FAILURE;
    }

    pthread_mutex_unlock(&mutex_socket_km);

    cambiar_estado(proceso_inicial, ESTADO_READY, logger);
    agregar_a_ready(proceso_inicial);

    // Loop principal — detecta BSOD
    while (1) {
        if (blue_screen_of_death) {
            log_error(logger, "## BLUE SCREEN OF DEATH");
            destruir_todos_global();
            break;
        }
        sleep(1);
    }

    cerrar_conexion(socket_kernel_memory, logger);
    cerrar_conexion(servidor, logger);
    config_destroy(config);
    log_destroy(logger);

    return EXIT_FAILURE; // BSOD, La única forma en la que un KS termina, porque algo sale mal. 
}
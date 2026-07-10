#include "procesos.h"
#include "scheduler.h"
#include "IO_manager.h"
#include "utils/hilos.h"
#include "planificador.h"
#include "mutex_manager.h"
#include <commons/config.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

// ----------------------------------------- Globales -----------------------------------------

t_log* logger; //logger del Kernel Scheduler.
t_config* config; //config del Kernel Scheduler.

int socket_km_notificaciones; //socket para recibir notificaciones del KM. No necesita de un mutex porque solo lo usa escuchar_kernel_memory.
int socket_km_control;   //socket para mandar notificaciones al KM. No necesita de un mutex porque solo lo usa escuchar_kernel_memory.
int socket_kernel_memory_operaciones; //socket para mandar operaciones al KM.
pthread_mutex_t mutex_socket_km_operaciones;

t_list* p_activos_global; //lista que contiene todos los procesos "vivos".
pthread_mutex_t mutex_p_activos;

uint32_t proximo_pid = 0; //identificador del siguiente proceso por crear.
pthread_mutex_t mutex_pid;

sem_t sem_compactacion; //semáforo productor-consumidor del hilo dispatcher para que no mande procesos a cpus mientas estoy compactando.

bool desalojo_por_compactacion = false; //indica forma distinta de manejar los procesos a la hora de agregarlos a ready. False por default.
pthread_mutex_t mutex_desalojo_compactacion;

sem_t sem_desalojo_ok; //sincroniza desalojo de CPUs con la confirmación de compactación al KM.

char* algoritmo; //algortimo de planificación de procesos.
char** queues_algoritmos; //cola de algoritmos de planificación de procesos para cada prioridad (solo si estamos en CMN).

int cant_prioridades = 1; //1 por default. Cambia si el algoritmo de planificación es CMN.

uint32_t suspension_timeout; //tiempo máximo (en milisegundos) que puede pasar un proceso en estado BLOCK antes de ser suspendido.

sem_t sem_bsod; //forma de hacer notar que el Kernel Memory notificó corrupción en la memoria.

bool hay_desalojo_cmn = false; //indica que, si estamos usando el algoritmo de planificación de procesos CMN, hay o no desalojo entre procesos. False por default.

// ------------------------------- Identificar Operación a Realizar -------------------------------

void* atender_cpu(void* arg) {
    int socket_cpu = *(int*)arg; //bellissimo.
    free(arg);

    char id_cpu[MAX_ID_CPU] = {0};
    recibir_string(socket_cpu, id_cpu, sizeof(id_cpu));

    log_info(logger, "## CPU %s Conectada", id_cpu);

    agregar_cpu_libre(socket_cpu);

    while (1) {
        //recibo la operación.
        op_code opcode;
        if (recibir_opcode(socket_cpu, &opcode) <= 0) {
            log_warning(logger, "CPU %s desconectada", id_cpu);
            break;
        }

        //recibo el pid sobre el cual ejecutarla.
        uint32_t pid;
        recibir_uint32(socket_cpu, &pid);

        //con el pid encuentro al proceso.
        t_pcb* proceso = encontrar_proceso_global(pid);

        if (proceso == NULL) {
            log_error(logger, "## CPU %s: PID %u no encontrado (opcode %d recibido) — cerrando conexión", id_cpu, pid, opcode);
            break;
        }
        //llamo a la función de manejo propia de la operación.
        switch (opcode) {
            case KS_TICK_PROGRESS_CONTINUE:
                manejar_tick_progress(socket_cpu, proceso);
                break;

            case KS_SYSCALL_IO:
                manejar_syscall_io_cpu(socket_cpu, proceso);
                break;
            
            case KS_MUTEX_CREATE:
                manejar_syscall_mutex_create(socket_cpu, proceso);
                break;

            case KS_MUTEX_LOCK:
                 manejar_syscall_mutex_lock(socket_cpu, proceso);
                break;

            case KS_MUTEX_UNLOCK:
                manejar_syscall_mutex_unlock (socket_cpu, proceso);
                break;
            
            case KS_MEM_ALLOC:
                manejar_syscall_mem_alloc(socket_cpu, proceso);
                break;
            
            case KS_MEM_FREE:
                manejar_syscall_mem_free(socket_cpu, proceso);
                break;
                
            case KS_EXIT:
                manejar_syscall_exit(socket_cpu, proceso);
                break;

            case KS_INIT_PROC: 
                manejar_iniciar_proceso(socket_cpu, proceso);
                break;

            default:
                log_warning(logger, "Opcode desconocido desde CPU %d", socket_cpu);
                break;
        }
    }

    cancelar_timer(socket_cpu);

    olvidar_interrupcion(socket_cpu);

    // Si había un proceso corriendo en esta CPU, rescatarlo a READY.
    int32_t pid_en_cpu = obtener_pid_de_cpu(socket_cpu);
    if (pid_en_cpu >= 0)
        rescatar_proceso_de_cpu_desconectada((uint32_t)pid_en_cpu, socket_cpu, logger);

    close(socket_cpu);

    return NULL;
}

// ------------------------------------- Aceptar CPUs e IOs -------------------------------------

void* escuchar_conexiones(void* arg) {
    int servidor = *(int*) arg; //bellisimo.
    while (1) {
        int cliente = esperar_cliente_modulo(logger, servidor, "Kernel Scheduler");
        if (cliente == -1) {
            log_warning(logger, "No se pudo conectar de forma correcta al cliente. Salteando ciclo del bucle");
            continue;
        }

        int32_t id_modulo = handshake_servidor(cliente, logger);

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
        if (recibir_opcode(socket_km_notificaciones, &opcode) <= 0) {
            log_error(logger, "Kernel Memory desconectado");
            sem_post(&sem_bsod);
            break;
        }

        switch (opcode) {
            case KM_BSOD:
                log_error(logger, "## Kernel Memory reportó corrupción de memoria");
                sem_post(&sem_bsod);
                break;

            case KM_NOTIF_MEMORIA_LIBRE:
                intentar_reanudar_proceso();
                break;
            
             case KM_NOTIF_COMPACTACION_FIN:
                log_info(logger, "## Fin de compactación");

                sem_post(&sem_compactacion);
                
                intentar_reanudar_proceso();
                break;

            case KM_NOTIF_COMPACTAR:
                sem_wait(&sem_compactacion);

                pthread_mutex_lock(&mutex_desalojo_compactacion);

                desalojo_por_compactacion = true;

                pthread_mutex_unlock(&mutex_desalojo_compactacion);

                // Contar cuántas CPUs hay en ejecución y marcarlas para desalojar.
                int cpus_a_desalojar = 0;

                pthread_mutex_lock(&mutex_exec);

                cpus_a_desalojar = list_size(lista_exec);
                for (int i = 0; i < cpus_a_desalojar; i++) {
                    t_pcb* en_exec = list_get(lista_exec, i);
                    int socket_cpu_exec = obtener_socket_cpu_de(en_exec->pid);
                    if (socket_cpu_exec != -1)
                        marcar_interrupcion(socket_cpu_exec);
                }
                pthread_mutex_unlock(&mutex_exec);

                //espero a que TODAS las CPUs marcadas confirmen su desalojo. Cada una hace sem_post(&sem_desalojo_ok) en manejar_tick_progress.
                for (int i = 0; i < cpus_a_desalojar; i++)
                    sem_wait(&sem_desalojo_ok);

                log_info(logger, "## Inicio de compactación");

                //confirmo al KM que puede compactar.
                enviar_opcode(socket_km_control, KM_COMPACTACION_OK);

                pthread_mutex_lock(&mutex_desalojo_compactacion);

                desalojo_por_compactacion = false;
                
                pthread_mutex_unlock(&mutex_desalojo_compactacion);
                break;
            
            default:
                log_warning(logger, "Opcode inesperado de KM: %d", opcode);
                break;
        }
    }
    return NULL;
}

static void inicializar_ks_estructuras() {
    p_activos_global = list_create();

    pthread_mutex_init(&mutex_pid, NULL);
    pthread_mutex_init(&mutex_p_activos, NULL);
    pthread_mutex_init(&mutex_desalojo_compactacion, NULL);
    pthread_mutex_init(&mutex_socket_km_operaciones, NULL);

    sem_init(&sem_compactacion, 0, 1);
    sem_init(&sem_desalojo_ok, 0, 0);
    sem_init(&sem_bsod, 0, 0);
}

//------------------------------------ MAIN KERNEL SCHEDULER ------------------------------------

int main(int argc, char* argv[]) {

    if (argc < 3) {
        printf("Uso: %s [config] [path_proceso_inicial]\n", argv[0]);
        return EXIT_FAILURE;
    }

    //crear el Config.
    config = config_create(argv[1]);
    if (config == NULL) {
        printf("Error al leer config\n");
        return EXIT_FAILURE;
    }

    //crear el Logger.
    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    t_log_level log_level = log_level_from_string(log_level_str);
    logger = log_create("kernel_scheduler.log", "KERNEL", 1, log_level);

    //obtener el algoritmo de planificación.
    algoritmo = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
    if(strcmp(algoritmo, "CMN") == 0) {
        cant_prioridades = 0;
        queues_algoritmos = config_get_array_value(config, "QUEUES_ALGORITHMS");
        while (queues_algoritmos[cant_prioridades] != NULL) //clever
            cant_prioridades++;

        char* preemtion = config_get_string_value(config, "QUEUE_PREEMPTION");
        if(strcmp(preemtion, "TRUE")== 0)
            hay_desalojo_cmn = true;
    }

    //inicializar lo pertinente al Kernel Scheduler.
    inicializar_ks_estructuras();
    inicializar_ks_planificador();
    inicializar_ks_mutex_manager();
    inicializar_ks_cpu_manager();
    inicializar_io_manager();

    //obtener tiempo máximo que puede un proceso estar bloqueado.
    suspension_timeout = config_get_int_value(config, "SUSPENSION_TIMEOUT");

    //conectar con Kernel Memory (operaciones y notificaciones).
    char* ip_km = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_km = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    socket_kernel_memory_operaciones = conectar_a_modulo(logger, ip_km, puerto_km, "Kernel Memory");

    if (socket_kernel_memory_operaciones == -1) {
        log_error(logger, "No se pudo conectar al Kernel Memory");
        return EXIT_FAILURE;
    }

    if (handshake_cliente(socket_kernel_memory_operaciones, logger, MODULO_KERNEL_SCHEDULER) == -1) {
        log_error(logger, "Handshake con Kernel Memory fallido");
        return EXIT_FAILURE;
    }
    log_info(logger, "## Conectado a Kernel Memory");

    socket_km_notificaciones = conectar_a_modulo(logger, ip_km, puerto_km, "Kernel Memory notificaciones");

    if (socket_km_notificaciones == -1) {
        log_error(logger, "No se pudo conectar al Notificador del Kernel Memory");
        return EXIT_FAILURE;
    }

    if (handshake_cliente(socket_km_notificaciones, logger, MODULO_KERNEL_SCHEDULER) == -1) {
        log_error(logger, "Handshake con Notificador del Kernel Memory fallido");
        return EXIT_FAILURE;
    }
    log_info(logger, "## Conectado al Notificador del Kernel Memory");

    socket_km_control = conectar_a_modulo(logger, ip_km, puerto_km, "Kernel Memory (control)");

    if (socket_km_control == -1) {
        log_error(logger, "No se pudo conectar al canal de control del Kernel Memory");
        return EXIT_FAILURE;
    }

    if (handshake_cliente(socket_km_control, logger, MODULO_KERNEL_SCHEDULER) == -1) {
        log_error(logger, "Handshake del canal de control fallido");
        return EXIT_FAILURE;
    }

    log_info(logger, "## Conectado al canal de control del Kernel Memory");

    //iniciar servidor para CPUs e IOs.
    char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    int servidor = iniciar_servidor_modulo(logger, puerto, "Kernel Scheduler");
    if (servidor == -1) {
        log_error(logger, "Fallo al crear servidor");
        return EXIT_FAILURE;
    }

    //arrancar hilo dispatcher, hilo servidor e hilo notificador.
    crear_hilo(escuchar_conexiones, &servidor);
    crear_hilo(escuchar_kernel_memory, NULL);
    crear_hilo(hilo_dispatcher, NULL);

    //crear proceso inicial (PID 0).
    crear_proceso(argv[2], PRIORIDAD_MAXIMA);
    //termina la ejecución normal del main.

    //espero que el kernel memory notifique corrupción de memoria (BSOD).
    sem_wait(&sem_bsod);
    
    log_error(logger, "## BLUE SCREEN OF DEATH");
    destruir_todos_global();

    cerrar_conexion(socket_kernel_memory_operaciones, logger);
    cerrar_conexion(servidor, logger);
    config_destroy(config);
    log_destroy(logger);

    return EXIT_FAILURE; //BSOD, La única forma en la que un KS termina, porque algo sale mal. 
}
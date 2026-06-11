#include "kernelmemory.h"
#include "utils/conexion.h"
#include <commons/collections/list.h>
#include <stdint.h>
#include <stdio.h>

static int socket_ks_notificaciones = -1;
static int socket_kernel_scheduler = -1;
static int socket_ks_operaciones = -1;

void inicializar_contexto(t_contexto* contexto) {
    contexto->pc = 0;
    contexto->ax = 0;
    contexto->bx = 0;
    contexto->cx = 0;
    contexto->dx = 0;
    contexto->eax = 0;
    contexto->ebx = 0;
    contexto->ecx = 0;
    contexto->edx = 0;
    contexto->si = 0;
    contexto->di = 0;
    contexto->tabla_segmentos = list_create();
}

t_list* leer_instrucciones(char* path)
{
    FILE* archivo = fopen(path, "r");
    if(archivo == NULL) return NULL;

    t_list* instrucciones = list_create();
    char linea[256];                 //mmm magic number BUFFER_SIZE?
    while(fgets(linea, sizeof(linea), archivo))
    {
        linea[strcspn(linea, "\n")] = '\0';

        list_add(instrucciones, strdup(linea));
    }
    fclose(archivo);
    return instrucciones;
}

void crear_proceso(int cliente, t_dictionary* procesos, t_log* logger)
{
    uint32_t pid;
    char path[256];                 //mmm magic number BUFFER_SIZE?

    recibir_uint32(cliente, &pid);
    recibir_string(cliente, path, sizeof(path));

    t_proceso_memoria* proceso = malloc(sizeof(t_proceso_memoria));

    proceso->pid = pid;
    inicializar_contexto(&(proceso->contexto));   
    proceso->instrucciones = leer_instrucciones(path); 

    char key[20];                   //mmm magic number KEY_SIZE?
    sprintf(key, "%d", pid);
    dictionary_put(procesos, key, proceso);
    log_info(logger, "## PID: %d - Proceso Creado", pid);

    enviar_opcode(cliente, RESPUESTA_OK);
}

void enviar_instruccion(int cliente, t_dictionary* procesos, t_log* logger)
{
    uint32_t pid;
    uint32_t pc;

    recibir_uint32(cliente, &pid);
    recibir_uint32(cliente, &pc);

    char key[20];
    sprintf(key, "%d", pid);
    t_proceso_memoria* proceso = dictionary_get(procesos, key);

    char* instruccion = list_get(proceso->instrucciones, pc);
    if(instruccion == NULL)
    {
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }

    enviar_opcode(cliente, RESPUESTA_OK);
    enviar_string(cliente, instruccion);

    log_info(logger, "## PID: %d - Obtener instruccion: %d - Instruccion: %s", pid, pc, instruccion);
}

void enviar_contexto(int cliente, t_dictionary* procesos, t_log* logger)
{
    uint32_t pid;
    recibir_uint32(cliente, &pid);

    char key[20];
    sprintf(key, "%d", pid);
    t_proceso_memoria* proceso = dictionary_get(procesos, key);
    if(proceso == NULL)
    {
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }
    enviar_opcode(cliente, RESPUESTA_OK);
    /*
        MAS ADELANTE:
        - serializar t_contexto
        - incluir tabla de segmentos
    */
    enviar_contexto_serializado(cliente, &(proceso->contexto));
    log_info(logger, "Contexto enviado PID %d", pid);
}

void actualizar_contexto(int cliente, t_dictionary* procesos, t_log* logger)
{
    uint32_t pid;
    recibir_uint32(cliente, &pid);

    char key[20];
    sprintf(key, "%d", pid);
    t_proceso_memoria* proceso = dictionary_get(procesos, key);
    if(proceso == NULL)
    {
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }
    /*
        MAS ADELANTE:
        - deserializar contexto completo
        - actualizar segmentos
    */
    recibir_contexto_serializado(cliente, &(proceso->contexto));

    log_info(logger, "Contexto actualizado PID %d", pid);
    enviar_opcode(cliente, RESPUESTA_OK);
}

void responder_mem_read(int cliente, t_log* logger)
{
    /*
        CHECKPOINT 2:
        NO hay memoria real.

        Solo responder OK.

        MAS ADELANTE:
        - traducir direccion logica
        - buscar segmento
        - leer memory sticks
    */

    log_info(logger, "MEM_READ mock");

    enviar_opcode(cliente, RESPUESTA_OK);

    //si algo falla mandar RESPUESTA_ERROR.
}

void responder_mem_write(int cliente, t_log* logger)
{
    /*
        CHECKPOINT 2:
        NO hay memoria real.

        Solo responder OK.

        MAS ADELANTE:
        - traducir direccion logica
        - escribir en memory sticks
    */

    log_info(logger, "MEM_WRITE mock");

    enviar_opcode(cliente, RESPUESTA_OK);

    //si algo falla mandar RESPUESTA_ERROR.
}

void responder_espacio_libre(int cliente, t_log* logger)
{
    /*
        CHECKPOINT 2:
        Devuelve valor fijo mock.
    */
    uint32_t espacio_libre = 999999; //mmm magic number
    enviar_uint32(cliente, espacio_libre);
    log_info(logger, "Espacio libre enviado");
}

// =========== ATENDER AL CLIENTE - FUNCION ORQUESTADORA =======

void* atender_cliente(void* arg)
{
    t_args_cliente* args = (t_args_cliente*) arg;
    int cliente = args->socket;
    t_dictionary* procesos = args->procesos;
    t_list* memory_sticks = args->memory_sticks;
    free(args);

    int32_t modulo = handshake_servidor(cliente, logger);

    if(modulo == MODULO_MEMORY_STICK)
    {
        uint32_t tamanio;
        recibir_uint32(cliente, &tamanio);

        t_memory_stick* stick = malloc(sizeof(t_memory_stick));
        stick->socket = cliente;
        stick->tamanio = tamanio;
        list_add(memory_sticks, stick);
        
        log_info(logger, "Memory Stick registrado. Tamaño: %u", tamanio);

        op_code op;
        while (recibir_opcode(cliente, &op) > 0);

        //si llegamos acá es porque salió del while
        log_error(logger, "Memory Stick desconectado — notificando BSOD");
        notificar_bsod_al_scheduler();

        cerrar_conexion(cliente, logger);

        return NULL;
    }

    if (modulo == MODULO_KERNEL_SCHEDULER) {
    if (socket_ks_operaciones == -1) {
        socket_ks_operaciones = cliente;
    } else {
        socket_ks_notificaciones = cliente;
    }
}

    while(1)
    {
        op_code operacion;
        if(recibir_opcode(cliente, &operacion) <= 0) {
            log_error(logger, "Cliente desconectado");
            break;
        }
        log_info(logger, "Recibo operacion %d", operacion);
        switch(operacion)
        {
            case KM_CREAR_PROCESO:
                crear_proceso(cliente, procesos, logger);
                break;
            case KM_PEDIR_INSTRUCCION:
                enviar_instruccion(cliente, procesos, logger);
                break;
            case KM_PEDIR_CONTEXTO:
                enviar_contexto(cliente, procesos, logger);
                break;
            case KM_ACTUALIZAR_CONTEXTO:
                actualizar_contexto(cliente, procesos, logger);
                break;
            case KM_MEM_READ:
                responder_mem_read(cliente, logger);
                break;
            case KM_MEM_WRITE:
                responder_mem_write(cliente, logger);
                break;
            case KM_ESPACIO_LIBRE:
                responder_espacio_libre(cliente, logger);
                break;
            default:
                log_error(logger, "Operacion desconocida");
                break;
        }
    }

    cerrar_conexion(cliente, logger);
    return NULL;
}

void notificar_bsod_al_scheduler() {
    if (socket_kernel_scheduler != -1) {
        log_error(logger, "Memory Stick desconectado — notificando BSOD al Scheduler");
        enviar_opcode(socket_kernel_scheduler, KM_BSOD);
    }
}

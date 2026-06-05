#include "cpu.h"
#include "utils/conexion.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


int pid_actual = -1; 

static char* fetch(int fd_memory, uint32_t pc, t_log* logger) {

    enviar_opcode(fd_memory, KM_PEDIR_INSTRUCCION);
    enviar_uint32(fd_memory, (uint32_t) pid_actual);
    enviar_uint32(fd_memory, pc);

    char* instruccion = malloc(256); //mmm magic number
    if(instruccion == NULL) {
        return NULL;
    }

    op_code respuesta;
    if (recibir_opcode(fd_memory, &respuesta) <= 0 || respuesta == RESPUESTA_ERROR) {
        log_error(logger, "Error en FETCH para PID %d PC %d", pid_actual, pc);
        free(instruccion);
        return NULL;
    }

    recibir_string(fd_memory, instruccion, 256);

    log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid_actual, pc);
    return instruccion;
}

static void actualizar_contexto(int fd_memory, t_contexto* ctx) {
    enviar_opcode(fd_memory, KM_ACTUALIZAR_CONTEXTO);
    enviar_uint32(fd_memory, (uint32_t) pid_actual);
    enviar_contexto_serializado(fd_memory, ctx);
}

static void enviar_syscall(int fd_scheduler, t_instruccion inst, op_code opcode) {
    enviar_opcode(fd_scheduler, opcode);
    enviar_uint32(fd_scheduler, (uint32_t) pid_actual);
    enviar_uint32(fd_scheduler, (uint32_t) inst.tipo);
    enviar_string(fd_scheduler, inst.param1);
    enviar_string(fd_scheduler, inst.param2);
}

static int check_interrupt(int fd_scheduler) {
    enviar_opcode(fd_scheduler, KS_TICK_PROGRESS_CONTINUE);
    enviar_uint32(fd_scheduler, (uint32_t) pid_actual);
    uint32_t interrupcion;
    if(recibir_uint32(fd_scheduler, &interrupcion) <= 0) {
        return 1;
    }
        return interrupcion;
}


static int execute(t_instruccion inst, t_contexto* ctx, int fd_scheduler, int fd_memory, t_log* logger) {
    log_info(logger, "## PID: %d - Ejecutando: %s %s %s", pid_actual, instruccion_to_string(inst.tipo), inst.param1, inst.param2);
    int pc_modificado = 0;
    switch(inst.tipo) {
        case INST_NOOP:
            break;
        case INST_SET: {
            uint32_t valor = strtoul(inst.param2, NULL, 10);
            escribir_registro(ctx, inst.param1, valor);
            break;
        }
        case INST_SUM: {
            uint32_t a = leer_registro(ctx, inst.param1);
            uint32_t b = leer_registro(ctx, inst.param2);
            escribir_registro(ctx, inst.param1, a + b);
            break;
        }
        case INST_SUB: {
            uint32_t a = leer_registro(ctx, inst.param1);
            uint32_t b = leer_registro(ctx, inst.param2);
            escribir_registro(ctx, inst.param1, a - b);
            break;
        }
        case INST_JNZ: {
            uint32_t valor = leer_registro(ctx, inst.param1);
            if(valor != 0) {
                ctx->pc = strtoul(inst.param2, NULL, 10);
                pc_modificado = 1;
            }
            break;
        }
        case INST_MUTEX_CREATE: {
            ctx->pc++;
            actualizar_contexto(fd_memory, ctx);
            enviar_syscall(fd_scheduler, inst, KS_MUTEX_CREATE);
            return 1;
        }
        case INST_MUTEX_LOCK: {
            ctx->pc++;
            actualizar_contexto(fd_memory, ctx);
            enviar_syscall(fd_scheduler, inst, KS_MUTEX_LOCK);
            return 1;
        }
        case INST_MUTEX_UNLOCK: {
            ctx->pc++;
            actualizar_contexto(fd_memory, ctx);
            enviar_syscall(fd_scheduler, inst, KS_MUTEX_UNLOCK);
            return 1;
        }
        case INST_MEM_ALLOC:
        case INST_MEM_FREE:
        case INST_SLEEP:
        case INST_STDOUT:
        case INST_STDIN: {
            if(!pc_modificado) {
                ctx->pc++;
            }
            actualizar_contexto(fd_memory, ctx);
            enviar_syscall(fd_scheduler, inst, KS_SYSCALL_IO);
            return 1;
        }
        case INST_INIT_PROC: {
            ctx->pc++;
            actualizar_contexto(fd_memory, ctx);
            enviar_opcode(fd_scheduler, KS_INIT_PROC);
            enviar_uint32(fd_scheduler, (uint32_t)pid_actual);
            enviar_string(fd_scheduler, inst.param1);
            enviar_uint32(fd_scheduler, (uint32_t)atoi(inst.param2));
            return 1;
        }
        case INST_EXIT: {
            if(!pc_modificado) {
                ctx->pc++;
            }
            actualizar_contexto(fd_memory, ctx);
            enviar_syscall(fd_scheduler, inst, KS_EXIT);
            return 1;
        }
        default:
            log_error(logger, "Instrucción desconocida");
            return 1; //1 indica syscall, no debería ser un exit_error o algo por el estilo?
    }
    if(!pc_modificado) {
        ctx->pc++;
    }
    return 0;
}

void ciclo_instruccion(int fd_scheduler, int fd_memory, t_log* logger) {
    t_contexto ctx;
    enviar_opcode(fd_memory, KM_PEDIR_CONTEXTO);
    enviar_uint32(fd_memory, (uint32_t) pid_actual);

    op_code respuesta;
    if (recibir_opcode(fd_memory, &respuesta) <= 0 || respuesta == RESPUESTA_ERROR) {
        log_error(logger, "Error al obtener contexto para PID %d", pid_actual);
        return;
    }

    recibir_contexto_serializado(fd_memory, &ctx);

    int ejecutando = 1;
    while(ejecutando) {
        char* texto = fetch(
            fd_memory,
            ctx.pc,
            logger
        );

        if(texto == NULL) {
            log_error(logger, "Error en FETCH");
            break;
        }

        // DECODE 
        t_instruccion inst = decode(texto);
        free(texto);

        // EXECUTE 
        int fue_syscall = execute(inst, &ctx, fd_scheduler, fd_memory, logger); // podría ser un opcode en vez de un bool de c
        if(fue_syscall) { 
            break;
        }

        // CHECK INTERRUPT 
        int interrupcion = check_interrupt(fd_scheduler);
        if(interrupcion) {
            log_info( logger, "## Interrupción recibida");
            actualizar_contexto(fd_memory,&ctx );
            break;
        }
    }
}

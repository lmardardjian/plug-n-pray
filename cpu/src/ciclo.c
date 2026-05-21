#include "cpu.h"
#include <utils/conexion.h>
#include <utils/mensajes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include "ciclo.h"


int pid_actual = -1;

// ── FETCH ──────────────────────────────────────────────────────
static char* fetch(int fd_memory, uint32_t pc, t_log* logger) {
    enviar_opcode(fd_memory, KM_PEDIR_INSTRUCCION);
    enviar_uint32(fd_memory, (uint32_t)pid_actual);
    enviar_uint32(fd_memory, pc);
    
    char* buffer = malloc(256);
    recibir_string(fd_memory, buffer, 256);

    log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid_actual, pc);
    return buffer;
}

// ── EXECUTE ────────────────────────────────────────────────────
static int execute(char* instruccion, t_contexto* ctx, int fd_scheduler, int fd_memory, t_log* logger) {

    char nombre[32] = {0};
    char param1[32] = {0};
    char param2[32] = {0};
    sscanf(instruccion, "%s %s %s", nombre, param1, param2);

    log_info(logger, "## PID: %d - Ejecutando: %s %s %s",
             pid_actual, nombre, param1, param2);

    int pc_modificado = 0;

    // ── Instrucciones normales ──
    if (strcmp(nombre, "NOOP") == 0) {
        // No hace nada

    } else if (strcmp(nombre, "SET") == 0) {
        uint32_t valor = (uint32_t)atoi(param2);
        escribir_registro(ctx, param1, valor);

    } else if (strcmp(nombre, "SUM") == 0) {
        uint32_t a = leer_registro(ctx, param1);
        uint32_t b = leer_registro(ctx, param2);
        escribir_registro(ctx, param1, a + b);

    } else if (strcmp(nombre, "SUB") == 0) {
        uint32_t a = leer_registro(ctx, param1);
        uint32_t b = leer_registro(ctx, param2);
        escribir_registro(ctx, param1, a - b);

    } else if (strcmp(nombre, "JNZ") == 0) {
        uint32_t val = leer_registro(ctx, param1);
        if (val != 0) {
            ctx->pc = (uint32_t)atoi(param2);
            pc_modificado = 1;
        }

    // ── Syscalls ──
    } else if (strcmp(nombre, "EXIT")         == 0 ||
               strcmp(nombre, "SLEEP")        == 0 ||
               strcmp(nombre, "STDOUT")       == 0 ||
               strcmp(nombre, "STDIN")        == 0 ||
               strcmp(nombre, "MUTEX_CREATE") == 0 ||
               strcmp(nombre, "MUTEX_LOCK")   == 0 ||
               strcmp(nombre, "MUTEX_UNLOCK") == 0 ||
               strcmp(nombre, "MEM_ALLOC")    == 0 ||
               strcmp(nombre, "MEM_FREE")     == 0 ||
               strcmp(nombre, "INIT_PROC")    == 0) {

        // Avisamos al Kernel Scheduler
        enviar_opcode(fd_scheduler, IO_EJECUTAR);
        enviar_uint32(fd_scheduler, (uint32_t)pid_actual);
        enviar_string(fd_scheduler, nombre);
        enviar_string(fd_scheduler, param1);
        enviar_string(fd_scheduler, param2);

        // Actualizamos el contexto en Kernel Memory
        enviar_opcode(fd_memory, KM_ACTUALIZAR_CONTEXTO);
        enviar_uint32(fd_memory, (uint32_t)pid_actual);
        enviar_contexto_serializado(fd_memory, ctx);

        return 1; // es syscall
    }

    if (!pc_modificado) {
        ctx->pc++;
    }

    return 0;
}

// ── CHECK INTERRUPT ────────────────────────────────────────────
static int hay_interrupcion(int fd_scheduler) {
    op_code codigo;
    int r = recv(fd_scheduler, &codigo, sizeof(op_code), MSG_DONTWAIT);
    return (r > 0);
}

// ── CICLO PRINCIPAL ────────────────────────────────────────────
void ciclo_instruccion(int fd_scheduler, int fd_memory, t_log* logger) {
    t_contexto ctx;

    // Pedir contexto
    enviar_opcode(fd_memory, KM_PEDIR_CONTEXTO);
    enviar_uint32(fd_memory, (uint32_t)pid_actual);
    recibir_contexto_serializado(fd_memory, &ctx);

    int corriendo = 1;
    while (corriendo) {

        // FETCH
        char* instruccion = fetch(fd_memory, ctx.pc, logger);

        // DECODE + EXECUTE
        int fue_syscall = execute(instruccion, &ctx, fd_scheduler, fd_memory, logger);
        free(instruccion);

        if (fue_syscall) {
            corriendo = 0;
            break;
        }

        // CHECK INTERRUPT
        if (hay_interrupcion(fd_scheduler)) {
            log_info(logger, "## Interrupción recibida");
            enviar_opcode(fd_memory, KM_ACTUALIZAR_CONTEXTO);
            enviar_uint32(fd_memory, (uint32_t)pid_actual);
            enviar_contexto_serializado(fd_memory, &ctx);
            corriendo = 0;
        }
    }
}
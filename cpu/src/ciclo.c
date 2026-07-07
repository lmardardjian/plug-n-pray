#include "cpu.h"
#include "utils/conexion.h"
#include "utils/constantes.h"
#include <commons/collections/list.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


int pid_actual = -1; 
uint32_t tam_max_segmento = 0;

// -------------------------------------------------------------------
// Tamaño en bytes de cada registro (para MOV_IN / MOV_OUT)
// AX/BX/CX/DX son de 1 byte, el resto son de 4 bytes.
// -------------------------------------------------------------------
static uint32_t tamanio_registro(char* nombre) {
    if (strcmp(nombre, "AX") == 0 || strcmp(nombre, "BX") == 0 ||
        strcmp(nombre, "CX") == 0 || strcmp(nombre, "DX") == 0) {
        return 1;
    }
    return 4;
}

// -------------------------------------------------------------------
// MMU: traduce una dirección lógica a una dirección física.
//
//   num_segmento   = dir_logica / tam_max_segmento
//   desplazamiento = dir_logica % tam_max_segmento
//   dir_fisica     = segmento.base + desplazamiento
//
// Devuelve la dirección física, o -1 si hay SEGMENTATION FAULT
// (el segmento no existe o el acceso se sale de su límite).
// -------------------------------------------------------------------
static int32_t mmu_traducir(t_contexto* ctx, uint32_t dir_logica, uint32_t tamanio) {
    uint32_t num_segmento   = dir_logica / tam_max_segmento;
    uint32_t desplazamiento = dir_logica % tam_max_segmento;

    t_segmento* segmento = NULL;
    for (int i = 0; i < list_size(ctx->tabla_segmentos); i++) {
        t_segmento* s = list_get(ctx->tabla_segmentos, i);
        if (s->id_segmento == num_segmento) {
            segmento = s;
            break;
        }
    }

    if (segmento == NULL) {
        return -1;
    }
    if (desplazamiento + tamanio > segmento->limite) {
        return -1;
    }

    return (int32_t)(segmento->base + desplazamiento);
}

// -------------------------------------------------------------------
// Lectura de memoria a través del Kernel Memory.
// CPU -> KM: KM_MEM_READ | pid | dir_fisica | tamanio
// KM  -> CPU: RESPUESTA_OK/RESPUESTA_ERROR | <tamanio bytes>
// -------------------------------------------------------------------
static int mem_read(int fd_memory, uint32_t dir_fisica, void* destino, uint32_t tamanio, t_log* logger) {
    enviar_opcode(fd_memory, KM_MEM_READ);
    enviar_uint32(fd_memory, (uint32_t)pid_actual);
    enviar_uint32(fd_memory, dir_fisica);
    enviar_uint32(fd_memory, tamanio);

    op_code respuesta;
    if (recibir_opcode(fd_memory, &respuesta) <= 0 || respuesta == RESPUESTA_ERROR) {
        log_error(logger, "PID: %d - Error en lectura de memoria - Dirección Física: %u", pid_actual, dir_fisica);
        return -1;
    }

    recibir_buffer(fd_memory, destino, tamanio);
    return 0;
}

// -------------------------------------------------------------------
// Escritura de memoria a través del Kernel Memory.
// CPU -> KM: KM_MEM_WRITE | pid | dir_fisica | tamanio | <tamanio bytes>
// KM  -> CPU: RESPUESTA_OK/RESPUESTA_ERROR
// -------------------------------------------------------------------
static int mem_write(int fd_memory, uint32_t dir_fisica, void* datos, uint32_t tamanio, t_log* logger) {
    enviar_opcode(fd_memory, KM_MEM_WRITE);
    enviar_uint32(fd_memory, (uint32_t)pid_actual);
    enviar_uint32(fd_memory, dir_fisica);
    enviar_uint32(fd_memory, tamanio);
    enviar_buffer(fd_memory, datos, tamanio);

    op_code respuesta;
    if (recibir_opcode(fd_memory, &respuesta) <= 0 || respuesta == RESPUESTA_ERROR) {
        log_error(logger, "PID: %d - Error en escritura de memoria - Dirección Física: %u", pid_actual, dir_fisica);
        return -1;
    }
    return 0;
}

static char* fetch(int fd_memory, uint32_t pc, t_log* logger) {

    enviar_opcode(fd_memory, KM_PEDIR_INSTRUCCION);
    enviar_uint32(fd_memory, (uint32_t) pid_actual);
    enviar_uint32(fd_memory, pc);

    char* instruccion = malloc(BUFFER_SIZE);
    
    if(instruccion == NULL) {
        return NULL;
    }

    op_code respuesta;
    if (recibir_opcode(fd_memory, &respuesta) <= 0 || respuesta == RESPUESTA_ERROR) {
        log_error(logger, "Error en FETCH para PID %d PC %d", pid_actual, pc);
        free(instruccion);
        return NULL;
    }

    recibir_string(fd_memory, instruccion, BUFFER_SIZE);

    log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid_actual, pc);
    return instruccion;
}

static void actualizar_contexto(int fd_memory, t_contexto* ctx) {
    enviar_opcode(fd_memory, KM_ACTUALIZAR_CONTEXTO);
    enviar_uint32(fd_memory, (uint32_t) pid_actual);
    enviar_contexto_serializado(fd_memory, ctx);

    // El Kernel Memory responde con RESPUESTA_OK / RESPUESTA_ERROR.
    // Si no lo leemos acá, queda en el buffer del socket y desincroniza
    // la próxima lectura (ej: la respuesta a KM_PEDIR_CONTEXTO).
    op_code respuesta;
    recibir_opcode(fd_memory, &respuesta);
}

static void enviar_syscall(int fd_scheduler, t_instruccion inst, op_code opcode) {
    enviar_opcode(fd_scheduler, opcode);
    enviar_uint32(fd_scheduler, (uint32_t) pid_actual);
    enviar_uint32(fd_scheduler, (uint32_t) inst.tipo);
    enviar_string(fd_scheduler, inst.param1);
    enviar_string(fd_scheduler, inst.param2);
}

// -------------------------------------------------------------------
// Notifica al Kernel Scheduler que el Proceso debe finalizar con
// motivo de Segmentation Fault (SEG_FAULT), siguiendo el mismo
// protocolo que la instrucción EXIT (KS_EXIT).
// -------------------------------------------------------------------
static void enviar_seg_fault(int fd_scheduler, int fd_memory, t_contexto* ctx, t_log* logger) {
    log_warning(logger, "## PID: %d - Error: Segmentation Fault (SEG_FAULT)", pid_actual);

    ctx->pc++;
    actualizar_contexto(fd_memory, ctx);

    t_instruccion seg_fault;
    seg_fault.tipo = INST_EXIT;
    strncpy(seg_fault.param1, "SEG_FAULT", sizeof(seg_fault.param1) - 1);
    seg_fault.param1[sizeof(seg_fault.param1) - 1] = '\0';
    seg_fault.param2[0] = '\0';

    enviar_syscall(fd_scheduler, seg_fault, KS_EXIT);
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
    log_info(logger, "## PID: %d - Ejecutando: %s - %s %s", pid_actual, instruccion_to_string(inst.tipo), inst.param1, inst.param2);
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
        case INST_MOV_IN: {
            // Lee de memoria[SI] (tamaño = tamaño del registro destino)
            // y guarda el valor leído en el registro indicado por param1.
            uint32_t tamanio = tamanio_registro(inst.param1);
            int32_t dir_fisica = mmu_traducir(ctx, ctx->si, tamanio);
            if(dir_fisica < 0) {
                enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
                return 1;
            }

            uint8_t buffer[4] = {0};
            if(mem_read(fd_memory, (uint32_t)dir_fisica, buffer, tamanio, logger) < 0) {
                enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
                return 1;
            }

            uint32_t valor = 0;
            memcpy(&valor, buffer, tamanio);
            escribir_registro(ctx, inst.param1, valor);

            log_info(logger, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %u", pid_actual, dir_fisica, valor);
            break;
        }
        case INST_MOV_OUT: {
            // Escribe el valor del registro param1 (Registro Datos) en
            // memoria[DI] (tamaño = tamaño del registro).
            uint32_t tamanio = tamanio_registro(inst.param1);
            uint32_t valor = leer_registro(ctx, inst.param1);

            int32_t dir_fisica = mmu_traducir(ctx, ctx->di, tamanio);
            if(dir_fisica < 0) {
                enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
                return 1;
            }

            if(mem_write(fd_memory, (uint32_t)dir_fisica, &valor, tamanio, logger) < 0) {
                enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
                return 1;
            }

            log_info(logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %u", pid_actual, dir_fisica, valor);
            break;
        }
        case INST_COPY_MEM: {
            // Copia desde memoria[SI] hacia memoria[DI], la cantidad de
            // bytes indicada por el valor del registro param1.
            uint32_t tamanio = leer_registro(ctx, inst.param1);

            int32_t dir_origen = mmu_traducir(ctx, ctx->si, tamanio);
            int32_t dir_destino = mmu_traducir(ctx, ctx->di, tamanio);
            if(dir_origen < 0 || dir_destino < 0) {
                enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
                return 1;
            }

            void* buffer = malloc(tamanio);
            if(buffer == NULL) {
                log_error(logger, "PID: %d - Error de memoria al hacer COPY_MEM", pid_actual);
                return 1;
            }

            if(mem_read(fd_memory, (uint32_t)dir_origen, buffer, tamanio, logger) < 0) {
                free(buffer);
                enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
                return 1;
            }
            log_info(logger, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: <%u bytes>", pid_actual, dir_origen, tamanio);

            if(mem_write(fd_memory, (uint32_t)dir_destino, buffer, tamanio, logger) < 0) {
                free(buffer);
                enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
                return 1;
            }
            log_info(logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: <%u bytes>", pid_actual, dir_destino, tamanio);

            free(buffer);
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
        case INST_MEM_ALLOC: {
            if(!pc_modificado) ctx->pc++;
            actualizar_contexto(fd_memory, ctx);
            enviar_syscall(fd_scheduler, inst, KS_MEM_ALLOC);
            return 1;
        }
        case INST_MEM_FREE: {
            if(!pc_modificado) ctx->pc++;
            actualizar_contexto(fd_memory, ctx);
            enviar_syscall(fd_scheduler, inst, KS_MEM_FREE);
            return 1;
        }
        case INST_SLEEP: {
            uint32_t tiempo = (uint32_t)atoi(inst.param1);

            ctx->pc++;
            actualizar_contexto(fd_memory, ctx);

            snprintf(inst.param1, sizeof(inst.param1), "%u", tiempo);
            inst.param2[0] = '\0';

            enviar_syscall(fd_scheduler, inst, KS_SYSCALL_IO);
            return 1;
        }
        case INST_STDOUT: {
            uint32_t dir_logica = leer_registro(ctx, inst.param1);
            uint32_t tamanio    = leer_registro(ctx, inst.param2);
            int32_t dir_fisica  = mmu_traducir(ctx, dir_logica, tamanio);
    
            if (dir_fisica < 0) {
                enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
                return 1;
            }
    
            ctx->pc++;
            actualizar_contexto(fd_memory, ctx);
    
            snprintf(inst.param1, sizeof(inst.param1), "%d", dir_fisica);
            snprintf(inst.param2, sizeof(inst.param2), "%u", tamanio);
    
            enviar_syscall(fd_scheduler, inst, KS_SYSCALL_IO);
            return 1;
        }
        case INST_STDIN: {
            uint32_t dir_logica = leer_registro(ctx, inst.param1);
            uint32_t tamanio  = leer_registro(ctx, inst.param2);
            int32_t dir_fisica  = mmu_traducir(ctx, dir_logica, tamanio);
    
            if (dir_fisica < 0) {
            enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
            return 1;
            }

            ctx->pc++;
            actualizar_contexto(fd_memory, ctx);

            snprintf(inst.param1, sizeof(inst.param1), "%d", dir_fisica);
            snprintf(inst.param2, sizeof(inst.param2), "%u", tamanio);

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
            log_error(logger, "## PID: %d - Instrucción desconocida", pid_actual);
            enviar_seg_fault(fd_scheduler, fd_memory, ctx, logger);
            return 1;
    }
    if(!pc_modificado) {
        ctx->pc++;
    }
    return 0;
}

void ciclo_instruccion(int fd_scheduler, int fd_memory, t_log* logger) {
    t_contexto ctx;

    ctx.pc = 0;
    ctx.ax = ctx.bx = ctx.cx = ctx.dx = 0;
    ctx.eax = ctx.ebx = ctx.ecx = ctx.edx = 0;
    ctx.si = ctx.di = 0;
    ctx.tabla_segmentos = list_create();

    enviar_opcode(fd_memory, KM_PEDIR_CONTEXTO);
    enviar_uint32(fd_memory, (uint32_t) pid_actual);

    op_code respuesta;
    if (recibir_opcode(fd_memory, &respuesta) <= 0 || respuesta == RESPUESTA_ERROR) {
        log_error(logger, "Error al obtener contexto para PID %d", pid_actual);
        destruir_tabla_segmentos(ctx.tabla_segmentos);
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
        int fue_syscall = execute(inst, &ctx, fd_scheduler, fd_memory, logger);
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
    destruir_tabla_segmentos(ctx.tabla_segmentos);
}
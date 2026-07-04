#include "swap.h"
#include "utils/conexion.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

t_swap_file* swap_file_abrir(const char* path, uint32_t file_size, uint32_t block_size)
{
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd == -1) return NULL;

    // el archivo se asume que arranca libre; solo garantizamos que tenga
    // el tamanio configurado (no hace falta limpiar su contenido)
    if (ftruncate(fd, file_size) == -1) {
        close(fd);
        return NULL;
    }

    t_swap_file* swap = malloc(sizeof(t_swap_file));
    swap->fd = fd;
    swap->block_size = block_size;
    swap->file_size = file_size;
    swap->total_bloques = (block_size > 0) ? (file_size / block_size) : 0;

    return swap;
}

void swap_file_cerrar(t_swap_file* swap)
{
    if (!swap) return;
    close(swap->fd);
    free(swap);
}

int swap_file_leer_bloque(t_swap_file* swap, uint32_t num_bloque, void* destino)
{
    off_t offset = (off_t)num_bloque * swap->block_size;
    ssize_t leido = pread(swap->fd, destino, swap->block_size, offset);
    return (leido == (ssize_t)swap->block_size) ? 0 : -1;
}

int swap_file_escribir_bloque(t_swap_file* swap, uint32_t num_bloque, void* datos)
{
    off_t offset = (off_t)num_bloque * swap->block_size;
    ssize_t escrito = pwrite(swap->fd, datos, swap->block_size, offset);
    return (escrito == (ssize_t)swap->block_size) ? 0 : -1;
}

static void atender_lectura(int socket_km, t_swap_file* swap, t_log* logger)
{
    uint32_t num_bloque;
    recibir_uint32(socket_km, &num_bloque);

    void* buffer = malloc(swap->block_size);

    if (swap_file_leer_bloque(swap, num_bloque, buffer) == 0) {
        enviar_opcode(socket_km, RESPUESTA_OK);
        enviar_buffer(socket_km, buffer, swap->block_size);
        log_info(logger, "## Lectura del bloque: %u", num_bloque);
    } else {
        enviar_opcode(socket_km, RESPUESTA_ERROR);
        log_error(logger, "## Error al leer el bloque: %u", num_bloque);
    }

    free(buffer);
}

static void atender_escritura(int socket_km, t_swap_file* swap, t_log* logger)
{
    uint32_t num_bloque;
    recibir_uint32(socket_km, &num_bloque);

    void* buffer = malloc(swap->block_size);
    recibir_buffer(socket_km, buffer, swap->block_size);

    if (swap_file_escribir_bloque(swap, num_bloque, buffer) == 0) {
        enviar_opcode(socket_km, RESPUESTA_OK);
        log_info(logger, "## Escritura del bloque: %u", num_bloque);
    } else {
        enviar_opcode(socket_km, RESPUESTA_ERROR);
        log_error(logger, "## Error al escribir el bloque: %u", num_bloque);
    }

    free(buffer);
}

void atender_kernel_memory(int socket_km, t_swap_file* swap, t_log* logger)
{
    while (1) {
        op_code operacion;
        if (recibir_opcode(socket_km, &operacion) <= 0) {
            log_error(logger, "## Kernel Memory desconectado");
            break;
        }

        switch (operacion) {
            case SW_LEER:
                atender_lectura(socket_km, swap, logger);
                break;
            case SW_ESCRIBIR:
                atender_escritura(socket_km, swap, logger);
                break;
            default:
                log_error(logger, "## Operacion desconocida: %d", operacion);
                break;
        }
    }
}
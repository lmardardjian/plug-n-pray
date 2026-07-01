#ifndef SWAP_H
#define SWAP_H

#include "utils/mensajes.h"
#include <commons/log.h>
#include <stdint.h>

typedef struct {
    int fd;             // file descriptor del archivo de swap
    uint32_t block_size;
    uint32_t file_size;
    uint32_t total_bloques;
} t_swap_file;

// abre (creando si hace falta) el archivo de swap y lo deja del tamanio configurado
t_swap_file* swap_file_abrir(const char* path, uint32_t file_size, uint32_t block_size);
void swap_file_cerrar(t_swap_file* swap);

// lee/escribe exactamente 1 bloque (block_size bytes)
int swap_file_leer_bloque(t_swap_file* swap, uint32_t num_bloque, void* destino);
int swap_file_escribir_bloque(t_swap_file* swap, uint32_t num_bloque, void* datos);

// atiende los pedidos SW_LEER / SW_ESCRIBIR que llegan de Kernel Memory
void atender_kernel_memory(int socket_km, t_swap_file* swap, t_log* logger);

#endif
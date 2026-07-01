#include <stdio.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/log.h>
#include "utils/conexion.h"
#include "swap.h"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Uso: ./swap [Archivo Config]\n");
        return EXIT_FAILURE;
    }

    t_config* config = config_create(argv[1]);
    if (config == NULL) {
        printf("Error al leer config\n");
        return EXIT_FAILURE;
    }

    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    t_log* logger = log_create("swap.log", "SWAP", 1, log_level_from_string(log_level_str));

    char*    swap_file_path = config_get_string_value(config, "SWAP_FILE_PATH");
    uint32_t swap_file_size = (uint32_t)config_get_int_value(config, "SWAP_FILE_SIZE");
    uint32_t block_size     = (uint32_t)config_get_int_value(config, "BLOCK_SIZE");

    t_swap_file* swap = swap_file_abrir(swap_file_path, swap_file_size, block_size);
    if (swap == NULL) {
        log_error(logger, "No se pudo abrir el archivo de SWAP: %s", swap_file_path);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    char* ip     = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    int socket_km = conectar_a_modulo(logger, ip, puerto, "Kernel Memory");
    if (socket_km == -1) {
        swap_file_cerrar(swap);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    if (handshake_cliente(socket_km, logger, MODULO_SWAP) != 0) {
        cerrar_conexion(socket_km, logger);
        swap_file_cerrar(swap);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    // le informamos a Kernel Memory el tamanio de bloque y el tamanio total del SWAP
    enviar_uint32(socket_km, block_size);
    enviar_uint32(socket_km, swap_file_size);

    log_info(logger, "## Conectado a Kernel Memory");

    atender_kernel_memory(socket_km, swap, logger);

    cerrar_conexion(socket_km, logger);
    swap_file_cerrar(swap);
    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
#include "io.h"
#include "utils/conexion.h"
#include "utils/constantes.h"
#include <commons/config.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdlib.h>

void cierre_io(t_log *logger, int conexion, t_config *config)
{
    if(logger != NULL) log_info(logger, "Cerrando modulo IO");
    if(logger != NULL) log_info(logger, "Cerrando conexion con Kernel Scheduler");
    if(conexion != -1) cerrar_conexion(conexion, logger);
    if(logger != NULL) log_info(logger, "Liberando config");
    if(config != NULL) config_destroy(config);
    if(logger != NULL) log_info(logger, "Destruyendo logger");
    if(logger != NULL) log_destroy(logger);
}

int main(int argc, char* argv[]) {
    // validacion de argumentos
    if (argc < 3) {
        argc < 2 ? printf("Falta archivo de config\n") : printf("Falta tipo de IO\n");
        return EXIT_FAILURE;
    }
    // obtengo configuracion
    t_config* config = config_create(argv[1]);
    if (config == NULL) {
        printf("Error al leer config\n");
        return EXIT_FAILURE;
    }
    // creo logger
    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    t_log_level log_level = log_level_from_string(log_level_str);
    t_log* logger = log_create("io.log", "IO", 1, log_level);

    log_info(logger, "INICIANDO MODULO IO");
    // obtengo tipo
    tipo_io tipo = get_tipo_io(argv[2]);
    if (tipo == -1)
    {
        log_error(logger, "Tipo de IO invalido");
        cierre_io(logger, -1, config);
        return EXIT_FAILURE;
    }
    log_info(logger, "Tipo de interfaz IO: %s", tipo_io_to_string(tipo));

    // leo config
    char* ip = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char* puerto = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");
    log_info(logger, "IP Kernel Scheduler: %s", ip);
    log_info(logger, "Puerto Kernel Scheduler: %s", puerto);

    //conexion a kernel
    log_info(logger, "Intentando conectar con Kernel Scheduler...");
    int conexion = conectar_a_modulo(logger, ip, puerto, "Kernel_Scheduler");

    if (conexion == -1) {
        log_error(logger, "No se pudo conectar con Kernel Scheduler");
        cierre_io(logger, conexion, config);
        return EXIT_FAILURE;
    }

    log_info(logger, "Conexion establecida con Kernel Scheduler");
    log_info(logger, "## Conectado a Kernel Scheduler");

    // handshake
    int resultado_handshake = handshake_cliente(conexion, logger, MODULO_IO);

    if(resultado_handshake == -1) {
        log_error(logger, "Handshake fallido");
        cierre_io(logger, conexion, config);
        return EXIT_FAILURE;
    }
    log_info(logger, "Handshake realizado correctamente");

    enviar_tipo_io(conexion, tipo);  // manda TIPO_IO_SLEEP/STDIN/STDOUT 

    // LOOP PRINCIPAL
    log_info(logger, "IO Entrando en loop principal de espera");
    while (1)
    {
        op_code codigo;
        log_info(logger, "Esperando opcode del Kernel...");
        if(recibir_opcode(conexion, &codigo) <= 0)
        {
            log_error(logger, "Kernel Scheduler desconectado");
            break;
        }
        log_info(logger, "Opcode recibido: %d", codigo);
        
        if(codigo == IO_EJECUTAR)
        {
            uint32_t pid;

            // PID
            log_info(logger, "Recibiendo PID...");
            if(recibir_uint32(conexion, &pid) <= 0)
            {
                log_error(logger, "Error recibiendo PID");
                break;
            }
            log_info(logger, "PID recibido: %d", pid);

            // PARAMETROS
            log_info(logger, "Recibiendo parametros IO...");
            uint32_t msg_len;
            if(recibir_uint32(conexion, &msg_len) <= 0)
            {
                log_error(logger, "Error recibiendo longitud del mensaje IO");
                break;
            }
            char* mensaje = calloc(msg_len + 1, 1);
            if(msg_len > 0 && recv(conexion, mensaje, msg_len, MSG_WAITALL) <= 0)
            {
                log_error(logger, "Error recibiendo parametros IO");
                free(mensaje);
                break;
            }
            log_info(logger, "Parametros recibidos: %s", mensaje);

            // LOG OBLIGATORIO
            log_info(logger, "## PID: %d - Inicio de IO", pid);
            switch(tipo)
            {
                case TIPO_IO_SLEEP:
                    ejecutar_sleep(pid, mensaje, logger);
                    break;
                case TIPO_IO_STDOUT:
                    ejecutar_stdout(pid, mensaje, logger);
                    break;
                case TIPO_IO_STDIN:
                    ejecutar_stdin(pid, conexion, mensaje, logger);
                    break;
                default:
                    log_error(logger, "Tipo IO desconocido");
                    enviar_opcode(conexion, RESPUESTA_ERROR);
                    free(mensaje);
                    continue;
            }
            // LOG OBLIGATORIO
            log_info(logger, "## PID: %d - Fin de IO", pid);
            enviar_opcode(conexion, RESPUESTA_OK);
            log_info(logger, "## PID: %d - IO_FINALIZADA enviada", pid);

            free(mensaje);
        }
        else
        {
            log_error(logger, "Opcode desconocido");
            enviar_opcode(conexion, RESPUESTA_ERROR);
        }
    }
    log_info(logger, "IO Salio del loop principal de espera");
    // END LOOP PRINCIPAL

    // cierre
    cierre_io(logger, conexion, config);

    return EXIT_SUCCESS;
}
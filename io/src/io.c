#include "io.h"
#include "utils/conexion.h"
#include "utils/constantes.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

tipo_io get_tipo_io(char* tipo) {
    if(strcasecmp(tipo, "SLEEP") == 0) {

        return TIPO_IO_SLEEP;
    }
    else if(strcasecmp(tipo, "STDIN") == 0) {

        return TIPO_IO_STDIN;
    }
    else if(strcasecmp(tipo, "STDOUT") == 0) {

        return TIPO_IO_STDOUT;
    }
    return -1;
}

void ejecutar_sleep(int pid, char* mensaje, t_log* logger)
{
    int tiempo = atoi(mensaje);
    //LOG OBLIGATORIO
    log_info(logger, "## PID: %d - Haciendo sleep por %d milisegundos.", pid, tiempo);
    usleep(tiempo * 1000);
}

void ejecutar_stdout(int pid, char* mensaje, t_log* logger)
{
    printf("%s\n", mensaje);
    fflush(stdout);
    log_info(logger, "## PID: %d - %s", pid, mensaje);
}

void ejecutar_stdin(int pid, int conexion, char* mensaje, t_log* logger)
{
    int cantidad = atoi(mensaje);
    if(cantidad >= BUFFER_SIZE)
    {
        cantidad = BUFFER_SIZE - 1;
    }
    char input[BUFFER_SIZE];
    memset(input, 0, BUFFER_SIZE);
    log_info(logger, "## PID: %d - Ingrese %d caracteres:", pid, cantidad);

    fgets(input, BUFFER_SIZE, stdin);
    // Si sobrepasa el limite indicado (mensaje), se corta en ese caracter. Si le falta para llegar al limite, se le agregan \0s 
    // eliminar \n
    input[strcspn(input, "\n")] = '\0';
    // buffer final exacto
    char resultado[cantidad + 1];
    memset(resultado, '\0', cantidad + 1);
    strncpy(resultado, input, cantidad);
    
    enviar_string(conexion, resultado);
    log_info(logger, "Input enviado al Kernel para PID %d", pid);
}
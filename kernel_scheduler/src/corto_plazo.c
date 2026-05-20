#include <commons/collections/queue.h>
#include <stdlib.h>
#include "/home/utnso/Documents/tp-2026-1c-Bobby-Tables/kernel_scheduler/src/pcb.h"

t_queue *p_block = queue_create(void);

//recibimos la notifiación de pedido de uso de i/o por parte de un pid


//buscamos dónde está el proceso
t_pcb * proceso = encontrar_proceso(p_activos_global, pid_proceso);
if(proceso==NULL){
    printf("Error, no se encontró el proceso en la lista de procesos activos\n");
    return EXIT_FAILURE;
}

//cambiamos el estado del proceso a block
cambiar_estado(proceso, ESTADO_BLOCK);

//mandamos a ejecutar a algún otro proceso que esté en ready


//agregamos el proceso a la cola de bloqueados y enviamos los parámetros necesarios
queue_push(p_block, proceso->pid);
switch (tipo)
{
case "SLEEP":
    enviar_opcode(socket_io, tipo);
    enviar_int(socket_io, pid);
    enviar_mensaje(socket_io, int_to_string(a_dormir), logger);
    break;

case "STDOUT":
    enviar_opcode(socket_io, tipo);
    enviar_int(socket_io, pid);
    enviar_mensaje(socket_io, msj, logger);
    break;

case "STDIN":
    enviar_opcode(socket_io, IO_EJECUTAR);
    enviar_int(socket_io, pid);
    enviar_mensaje(socket_io, caract_por_leer, logger);
    break;

default:

    break;
}

//esperamos respuesta de i/o de que el proceso ya usó el recurso que pidió
//(este hace "pop" en la lista p_block)


//cambiamos el estado del proceso a ready
cambiar_estado(proceso, ESTADO_READY);

//mandamos el proceso a la lista de ready
agregar_a_ready(proceso->pid);

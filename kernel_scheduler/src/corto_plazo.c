#include <commons/collections/queue.h>
#include <stdlib.h>
#include "./scheduler.h"
#include "./pcb.h"

//recibimos la notifiación de pedido de uso de i/o por parte de una cpu junto con
//el i/o a usar y los parámetros.


//buscamos dónde está el proceso.
t_pcb *proceso = encontrar_proceso(p_activos_global, pid_proceso);
if(proceso==NULL){
    printf("Error, no se encontró el proceso en la lista de procesos activos\n");
    return EXIT_FAILURE;
}

//cambiamos el estado del proceso a block.
cambiar_estado(proceso, ESTADO_BLOCK);

//mandamos a ejecutar a otro proceso que esté en ready si es que lo hay.
if(!queue_is_empty(cola_ready)){
    //la cola no está vacía, mando un proceso a ejecutar.
}

//agregamos el proceso a la cola de bloqueados y enviamos los parámetros necesarios al i/o.

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

//esperamos respuesta de i/o de que el proceso ya usó el recurso que pidió.
//(i/o hace "pop" en cola_block)


//cambiamos el estado del proceso a ready.
cambiar_estado(proceso, ESTADO_READY);

//mandamos el proceso a la lista de ready.
agregar_a_ready(proceso->pid);

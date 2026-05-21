#include <commons/collections/queue.h>
#include <stdlib.h>
#include "./scheduler.h"
#include "./pcb.h"

//recibimos la notifiación de pedido de uso de i/o por parte de una cpu junto con
//el i/o a usar y los parámetros.

//cambiamos el estado del proceso a block.
cambiar_estado(proceso, ESTADO_BLOCK);

//mandamos a ejecutar a otro proceso que esté en ready si es que lo hay.
if(!queue_is_empty(cola_ready)){
    //la cola no está vacía, mando un proceso a ejecutar.
}

//agregamos el proceso a la cola de bloqueados y enviamos los parámetros necesarios al i/o.


//esperamos respuesta de i/o de que el proceso ya usó el recurso que pidió.
//(i/o hace "pop" en cola_block)


//cambiamos el estado del proceso a ready.
cambiar_estado(proceso, ESTADO_READY);

//mandamos el proceso a la lista de ready.
agregar_a_ready(proceso->pid);

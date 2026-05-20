#include <commons/collections/queue.h>
#include <pcb.h>

t_queue * p_running = queue_create(void);
t_queue * p_ready = queue_create(void);
t_queue * p_block = queue_create(void);


//recibimos la notifiación de pedido de uso de i/o por parte de un pid


//buscamos dónde está el proceso
t_pcb * proceso = encontrar_proceso(p_running, pid_proceso);

//cambiamos el estado del proceso a block
cambiar_estado(proceso, ESTADO_BLOCK);

//mandamos a ejecutar a algún otro proceso que esté en ready


//agregamos el proceso a la cola de bloqueados
queue_push(p_block, proceso->pid);

//esperamos respuesta de i/o de que el proceso ya usó el recurso que pidió
//(este hace "pop" en la lista p_block)


//cambiamos el estado del proceso a ready
cambiar_estado(proceso, ESTADO_READY);

//mandamos el proceso a la lista de ready


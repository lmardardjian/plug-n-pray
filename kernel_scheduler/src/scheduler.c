#include "scheduler.h"
#include <stdlib.h>

t_queue* cola_ready;
pthread_mutex_t mutex_ready;
sem_t sem_procesos_en_ready;

void inicializar_planificador() {
    cola_ready = queue_create();
    pthread_mutex_init(&mutex_ready, NULL);
    sem_init(&sem_procesos_en_ready, 0, 0);
}

//Las funciones deberían pedir el pid no la dirección del proceso. La dirección solo la
//debería tener p_activos_global.

void agregar_a_ready(t_pcb* proceso) {
    pthread_mutex_lock(&mutex_ready);   //Wait, cierra el candado
    queue_push(cola_ready, proceso);    // Ingresamos el proceso a READY
    pthread_mutex_unlock(&mutex_ready); //SIgnal, abre el candado
}

/*Hay que revisar esto. Es necesario hacer una función que llame a obtener_siguiente_proceso
teniendo en cuenta el planificador que se haya especificado. Si es FIFO no hay problema ya 
que queue_pop funca para colas, si es RR también funcionaría ya que su lista de procesos que
están en ready también es una cola, el problema es si el planificador es por prioridades
ya que habría que hacer uso de la función en cada cola desde la prioridad máxima y hasta la 
mínima si no hubiese elementos en las otras.*/

t_pcb* obtener_siguiente_proceso() {
    pthread_mutex_lock(&mutex_ready);       //Wait, cierra el candado
    t_pcb* proceso = queue_pop(cola_ready); //Sacamos el proceso de READY
    pthread_mutex_unlock(&mutex_ready);     //SIgnal, abre el candado
    return proceso;
}
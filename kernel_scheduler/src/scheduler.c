#include "scheduler.h"
#include <stdlib.h>

t_queue *cola_ready;
t_queue *cola_block;
t_list *lista_running;
pthread_mutex_t mutex_ready;
sem_t sem_procesos_en_ready;
pthread_mutex_t mutex_block;
sem_t sem_procesos_en_block;
pthread_mutex_t mutex_running;
sem_t sem_procesos_en_running;

void inicializar_planificador() {
    cola_ready = queue_create();
    cola_block = queue_create();
    lista_running = list_create();
    pthread_mutex_init(&mutex_ready, NULL);
    sem_init(&sem_procesos_en_ready, 0, 0);
    pthread_mutex_init(&mutex_block, NULL);
    sem_init(&sem_procesos_en_block, 0, 0);
    pthread_mutex_init(&mutex_running, NULL);
    sem_init(&sem_procesos_en_running, 0, 0);
}

void agregar_a_running(t_list* lista_running, uint32_t pid) {
    pthread_mutex_lock(&mutex_running);

    pthread_mutex_unlock(&mutex_running);
}

void quitar_de_running(t_list* lista_running, uint32_t pid) {
    pthread_mutex_lock(&mutex_running);
    
    pthread_mutex_unlock(&mutex_running);
}


void agregar_a_ready(t_queue* cola_ready, uint32_t pid) {
    pthread_mutex_lock(&mutex_ready);   //Wait, cierra el candado
    queue_push(cola_ready, pid);    // Ingresamos el proceso a READY
    pthread_mutex_unlock(&mutex_ready); //Signal, abre el candado
}

void quitar_de_ready(t_queue* cola_ready, uint32_t pid) { 
    pthread_mutex_lock(&mutex_ready);
    queue_pop(cola_ready, pid);
    pthread_mutex_unlock(&mutex_ready);
}

void agregar_a_block(t_queue* cola_block, uint32_t pid) { 
    pthread_mutex_lock(&mutex_block);
    queue_push(cola_block, pid);
    pthread_mutex_unlock(&mutex_block);
}

void quitar_de_block(t_queue* cola_block, uint32_t pid) { 
    pthread_mutex_lock(&mutex_block);
    queue_pop(cola_block, pid);
    pthread_mutex_unlock(&mutex_block);
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
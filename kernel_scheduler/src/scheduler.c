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

//Hay que revisar esto. Solo funcionaria con FIFO.

void agregar_a_ready(t_pcb* proceso) {
    pthread_mutex_lock(&mutex_ready);   //Wait, cierra el candado
    queue_push(cola_ready, proceso);    // Ingresamos el proceso a READY
    pthread_mutex_unlock(&mutex_ready); //SIgnal, abre el candado
}

t_pcb* obtener_siguiente_proceso() {
    pthread_mutex_lock(&mutex_ready);       //Wait, cierra el candado
    t_pcb* proceso = queue_pop(cola_ready); //Sacamos el proceso de READY
    pthread_mutex_unlock(&mutex_ready);     //SIgnal, abre el candado
    return proceso;
}
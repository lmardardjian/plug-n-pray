#include "scheduler.h"

t_queue* cola_ready;
t_queue* cola_block;
t_list* lista_exec;

pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_block;
pthread_mutex_t mutex_exec;

sem_t sem_procesos_en_ready;
sem_t sem_procesos_en_block;
sem_t sem_procesos_en_exec;

void inicializar_planificador() {
    cola_ready = queue_create();
    cola_block = queue_create();
    lista_exec = list_create();

    pthread_mutex_init(&mutex_ready, NULL);
    sem_init(&sem_procesos_en_ready, 0, 0);

    pthread_mutex_init(&mutex_block, NULL);
    sem_init(&sem_procesos_en_block, 0, 0);

    pthread_mutex_init(&mutex_exec, NULL);
    sem_init(&sem_procesos_en_exec, 0, 0);
}

// ------------------- READY -------------------------

void agregar_a_ready(t_pcb* proceso) {
    pthread_mutex_lock(&mutex_ready);   //Wait, cierra el candado
    queue_push(cola_ready, proceso);    // Ingresamos el proceso a READY
    pthread_mutex_unlock(&mutex_ready); //Signal, abre el candado
    sem_post(&sem_procesos_en_ready);   //Avisa que hay un proceso
}

t_pcb* obtener_siguiente_proceso() {
    sem_wait(&sem_procesos_en_ready);       //Señal de que hay proceso
    pthread_mutex_lock(&mutex_ready);       //Wait, cierra el candado
    t_pcb* proceso = queue_pop(cola_ready); //Sacamos el proceso de READY
    pthread_mutex_unlock(&mutex_ready);     //SIgnal, abre el candado
    return proceso;
}

// -------------- BLOCK -----------------------

void agregar_a_block(t_pcb* proceso) { 
    pthread_mutex_lock(&mutex_block);

    queue_push(cola_block, proceso);
    pthread_mutex_unlock(&mutex_block);
    sem_post(&sem_procesos_en_block);
}

t_pcb* quitar_de_block(uint32_t pid) { //Revisar
    pthread_mutex_lock(&mutex_block);

    t_pcb* encontrado = NULL;
    int tamanio = queue_size(cola_block);

    for (int i = 0; i < tamanio; i++) {
        t_pcb* proceso = queue_pop(cola_block);
        if (proceso->pid == pid && encontrado == NULL) {
            encontrado = proceso;
        } else {
            queue_push(cola_block, proceso);
        }
    }
    pthread_mutex_unlock(&mutex_block);
    return encontrado;
}

// ----------------- EXECUTE -----------------------

void agregar_a_exec(t_pcb* proceso) {
    pthread_mutex_lock(&mutex_exec);
    list_add(lista_exec, proceso);
    pthread_mutex_unlock(&mutex_exec);
}

void quitar_de_exec(uint32_t pid) {
    pthread_mutex_lock(&mutex_exec);
     for (int i = 0; i < list_size(lista_exec); i++) {
        t_pcb* proceso = list_get(lista_exec, i);
        if (proceso->pid == pid) {
            list_remove(lista_exec, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_exec);
}
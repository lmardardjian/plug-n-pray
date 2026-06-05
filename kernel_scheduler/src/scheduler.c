#include "scheduler.h"

static int cant_prioridades;
static uint32_t nivel_de_prioridad; 
pthread_mutex_t mutex_nivel_de_prioridad;

t_queue* cola_ready;
t_queue* lista_block;
t_list* lista_exec;
t_list* lista_susp_ready;
t_list* lista_susp_block;

pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_block;
pthread_mutex_t mutex_exec;
pthread_mutex_t mutex_susp_ready;
pthread_mutex_t mutex_susp_block;

sem_t sem_procesos_en_ready;

void inicializar_planificador() {
    cola_ready = queue_create();
    lista_block = list_create();
    lista_exec = list_create();
    lista_susp_ready = list_create();
    lista_susp_block = list_create();

    pthread_mutex_init(&mutex_ready, NULL);
    pthread_mutex_init(&mutex_block, NULL);
    pthread_mutex_init(&mutex_exec, NULL);
    pthread_mutex_init(&mutex_susp_ready, NULL);
    pthread_mutex_init(&mutex_susp_block, NULL);

    sem_init(&sem_procesos_en_ready, 0, 0);

    pthread_mutex_init(&mutex_nivel_de_prioridad, NULL);
}

// ------------------- READY --------------------

void agregar_a_ready(t_pcb* proceso) {
    pthread_mutex_lock(&mutex_ready);   //Wait, cierra el candado

    queue_push(cola_ready, proceso);    // Ingresamos el proceso a READY

    pthread_mutex_unlock(&mutex_ready); //Signal, abre el candado

    sem_post(&sem_procesos_en_ready);   //Avisa que hay un proceso
}

t_pcb* obtener_siguiente_proceso() {//tiene que cambiar con CMN
    sem_wait(&sem_procesos_en_ready);       //Señal de que hay proceso

    pthread_mutex_lock(&mutex_ready);       //Wait, cierra el candado

    t_pcb* proceso = queue_pop(cola_ready); //Sacamos el proceso de READY

    pthread_mutex_unlock(&mutex_ready);     //Signal, abre el candado
    return proceso;
}

// -------------- BLOCK -----------------------

void agregar_a_block(t_pcb* proceso) { 

    pthread_mutex_lock(&mutex_block);

    list_add(lista_block, proceso);

    pthread_mutex_unlock(&mutex_block);

    proceso->tiempo_susp = temporal_create();
}

t_pcb* quitar_de_block(uint32_t pid) {

    pthread_mutex_lock(&mutex_block);

    t_pcb* encontrado = NULL;
    int tamanio = list_size(lista_block);

    for (int i = 0; i < tamanio; i++) {
        t_pcb* proceso = list_get(lista_block, i);
        if (proceso->pid == pid) {
            encontrado = list_remove(lista_block, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_block);

    if(encontrado!=NULL)
    temporal_destroy(encontrado->tiempo_susp);

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

// ----------------- SUSP. READY -----------------------

void agregar_a_susp_ready(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_susp_ready);

    list_add(lista_susp_ready, proceso);

    pthread_mutex_unlock(&mutex_susp_ready);
}

t_pcb* quitar_de_susp_ready(uint32_t pid) {

    pthread_mutex_lock(&mutex_susp_ready);

    t_pcb* resultado = NULL;
    for (int i = 0; i < list_size(lista_susp_ready); i++) {
        t_pcb* proceso = list_get(lista_susp_ready, i);
        if (proceso->pid == pid) {
            resultado = list_remove(lista_susp_ready, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_susp_ready);

    return resultado;
}

// ----------------- SUSP. BLOCK -----------------------

void agregar_a_susp_block(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_susp_block);

    list_add(lista_susp_block, proceso);

    pthread_mutex_unlock(&mutex_susp_block);
}

t_pcb* quitar_de_susp_block() {
    t_list* lista_prioridades = NULL;
    t_pcb* proceso = NULL;

    pthread_mutex_lock(&mutex_susp_block);

    pthread_mutex_lock(&mutex_nivel_de_prioridad);

    for(nivel_de_prioridad = 0; nivel_de_prioridad < cant_prioridades; nivel_de_prioridad++) { 
        lista_prioridades = list_filter(lista_susp_block, prioridad_igual_a);
        if(list_size(lista_prioridades) == 0) {
            list_destroy(lista_prioridades);
            continue;
        }
        else if(list_size(lista_prioridades) == 1) {
        proceso = list_remove_by_condition(lista_susp_block, prioridad_igual_a);
        break;
        }
        else {
            proceso = mas_tiempo_en_susp_block(lista_prioridades);
            list_remove_element(lista_susp_block, proceso);
            break;            
        }
    }
    pthread_mutex_unlock(&mutex_nivel_de_prioridad);

    pthread_mutex_unlock(&mutex_susp_block);

    return proceso;
}

bool prioridad_igual_a(void * ptr) {
    t_pcb* proceso = (t_pcb*) ptr;
    return proceso->prioridad == nivel_de_prioridad;
}

t_pcb* mas_tiempo_en_susp_block(t_list* lista_prioridades) {

    return (t_pcb*) list_get_minimum(lista_prioridades, mas_antiguo_en_susp);
}

void* mas_antiguo_en_susp(void* a, void* b) {
    t_pcb* proceso_a = (t_pcb*) a;
    t_pcb* proceso_b = (t_pcb*) b;

    temporal_stop(proceso_a->tiempo_susp);
    temporal_stop(proceso_b->tiempo_susp);

    if(temporal_gettime(proceso_a->tiempo_susp) >= temporal_gettime(proceso_b->tiempo_susp)) {
        temporal_resume(proceso_a->tiempo_susp);
        temporal_resume(proceso_b->tiempo_susp);
        return proceso_a;
    }
    else {
        temporal_resume(proceso_a->tiempo_susp);
        temporal_resume(proceso_b->tiempo_susp);
        return proceso_b;
    }
}

    /*
        t_pcb* proceso va a lista_block
        (proceso->tiempo_susp) = temporal_create()

        (proceso->tiempo_susp) va a lista_clock con función "agregar_a_clock" contiguamente a "agregar_a_block"
        creo un hilo específico que pasado "x" tiempo se fija si el proceso sigue en block:

        sleep(x)
        temporal_stop(proceso->tiempo_susp)
        if(temporal_gettime(proceso->tiempo_susp)>x)
            temporal_resume(proceso->tiempo_susp)
            proceso sale de lista_block
            proceso va a lista_susp_block
        else
            list_remove_and_destroy_by_condition(lista_clock, , temporal_destroy)

    */
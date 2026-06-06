#include "scheduler.h"

//listas para cada estado (salvo ready por ahora que es cola)
t_queue* cola_ready;
t_queue* lista_block;
t_list* lista_exec;
t_list* lista_susp_ready;
t_list* lista_susp_block;

//mutexes para cada lista
pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_block;
pthread_mutex_t mutex_exec;
pthread_mutex_t mutex_susp_ready;
pthread_mutex_t mutex_susp_block;

//semáforo productor-consumidor de cola_ready
sem_t sem_procesos_en_ready;

void inicializar_ks_planificador() {        //tiene que cambiar con CMN
    //inicializo listas
    cola_ready = queue_create();
    lista_block = list_create();
    lista_exec = list_create();
    lista_susp_ready = list_create();
    lista_susp_block = list_create();

    //inicializo mutexes
    pthread_mutex_init(&mutex_ready, NULL);
    pthread_mutex_init(&mutex_block, NULL);
    pthread_mutex_init(&mutex_exec, NULL);
    pthread_mutex_init(&mutex_susp_ready, NULL);
    pthread_mutex_init(&mutex_susp_block, NULL);

    sem_init(&sem_procesos_en_ready, 0, 0);
}

// ------------------- READY --------------------

void agregar_a_ready(t_pcb* proceso) {

    pthread_mutex_lock(&mutex_ready);

    queue_push(cola_ready, proceso);

    pthread_mutex_unlock(&mutex_ready);

    sem_post(&sem_procesos_en_ready);   //Avisa que hay un proceso en cola_ready
}

t_pcb* obtener_siguiente_proceso() {        //tiene que cambiar con CMN
    
    sem_wait(&sem_procesos_en_ready);

    pthread_mutex_lock(&mutex_ready);

    t_pcb* proceso = queue_pop(cola_ready);

    pthread_mutex_unlock(&mutex_ready);

    return proceso;
}

// -------------- BLOCK -----------------------

void agregar_a_block(t_pcb* proceso) { 

    pthread_mutex_lock(&mutex_block);

    list_add(lista_block, proceso);

    pthread_mutex_unlock(&mutex_block);

    proceso->tiempo_susp = temporal_create();   //empieza el contador de tiempo en block

    crear_hilo(hilo_suspension, proceso);       //hilo encargado de, si el proceso está más de lo debido bloqueado, pasarlo a susp_block
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
    temporal_destroy(encontrado->tiempo_susp); //raro sería tratar de referenciar NULL, no? Jjasjajs

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

    pthread_mutex_lock(&mutex_susp_block);

    t_pcb* proceso = NULL;

    for (uint32_t nivel = 0; nivel < cant_prioridades && proceso == NULL; nivel++) { // este for recorre susp_block por nivel de prioridad

        t_pcb* candidato = NULL;
        for (int i = 0; i < list_size(lista_susp_block); i++) { //este for hace un list_filter a mano y la selección del proceso a reanudar
            t_pcb* p = list_get(lista_susp_block, i);
            if (p->prioridad != nivel)
                continue;

            if (candidato == NULL) {
                candidato = p;
            } else {
                temporal_stop(p->tiempo_susp);
                temporal_stop(candidato->tiempo_susp);

                bool p_es_mas_antiguo = temporal_gettime(p->tiempo_susp) >= temporal_gettime(candidato->tiempo_susp);
                
                temporal_resume(p->tiempo_susp);
                temporal_resume(candidato->tiempo_susp);

                if (p_es_mas_antiguo)
                    candidato = p;
            }
        }

        if (candidato != NULL) {
            list_remove_element(lista_susp_block, candidato);
            proceso = candidato;
        }
    }

    pthread_mutex_unlock(&mutex_susp_block);

    return proceso;
}

t_pcb* quitar_de_susp_block_por_pid(uint32_t pid) {

    pthread_mutex_lock(&mutex_susp_block);

    t_pcb* resultado = NULL;
    for (int i = 0; i < list_size(lista_susp_block); i++) {
        t_pcb* p = list_get(lista_susp_block, i);
        if (p->pid == pid) {
            resultado = list_remove(lista_susp_block, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_susp_block);

    return resultado;
}

void* hilo_suspension(void* arg) {
    t_pcb* proceso = (t_pcb*) arg;
    usleep(suspension_timeout * 1000);

    if (proceso->estado != ESTADO_BLOCK)
        return NULL;
        // El listener ya lo movió a READY — no hacer nada

    // Todavía en BLOCK — suspender
    quitar_de_block(proceso->pid);
    cambiar_estado(proceso, ESTADO_SUSP_BLOCK, logger);
    agregar_a_susp_block(proceso);

    // Notificar a KM
    pthread_mutex_lock(&mutex_socket_km);

    enviar_opcode(socket_kernel_memory, KM_SUSPENDER_PROCESO);
    enviar_uint32(socket_kernel_memory, proceso->pid);
    
    op_code ack;
    recibir_opcode(socket_kernel_memory, &ack);

    pthread_mutex_unlock(&mutex_socket_km);

    return NULL;
}

void intentar_reanudar_proceso() { //enfasis en "intentar"

    //primero intentar reanudar procesos en SUSP_READY

    pthread_mutex_lock(&mutex_susp_ready);

    for (int i = 0; i < list_size(lista_susp_ready); i++) {
        t_pcb* proceso = list_get(lista_susp_ready, i);

        pthread_mutex_lock(&mutex_socket_km);

        enviar_opcode(socket_kernel_memory, KM_REANUDAR_PROCESO);
        enviar_uint32(socket_kernel_memory, proceso->pid);

        op_code ack;
        recibir_opcode(socket_kernel_memory, &ack);

        pthread_mutex_unlock(&mutex_socket_km);

        if (ack == RESPUESTA_OK) {
            list_remove(lista_susp_ready, i);
            cambiar_estado(proceso, ESTADO_READY, logger);
            agregar_a_ready(proceso);
            i--;  //ajustar índice porque removimos un elemento
        }
        //si no hay memoria para este, intentamos con el siguiente
    }
    pthread_mutex_unlock(&mutex_susp_ready);

    //después intentar reanudar procesos en SUSP_BLOCK

    t_pcb* proceso = quitar_de_susp_block();
    if (proceso == NULL) 
        return;

    pthread_mutex_lock(&mutex_socket_km);

    enviar_opcode(socket_kernel_memory, KM_REANUDAR_PROCESO);
    enviar_uint32(socket_kernel_memory, proceso->pid);

    op_code ack;
    recibir_opcode(socket_kernel_memory, &ack);

    pthread_mutex_unlock(&mutex_socket_km);

    if (ack == RESPUESTA_OK) {
        cambiar_estado(proceso, ESTADO_READY, logger);
        agregar_a_ready(proceso);
    } else {
        agregar_a_susp_block(proceso);
    }
}
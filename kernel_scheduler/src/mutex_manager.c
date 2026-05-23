#include "procesos.h"
#include "scheduler.h"
#include "mutex_manager.h"
#include "utils/conexion.h"
#include <string.h>

extern t_log* logger;

t_list* lista_mutexes;

void inicializar_mutexes() {
    lista_mutexes = list_create();
}

//Crea Mutex

t_mutex_kernel* crear_mutex(char* nombre) {

    t_mutex_kernel* nuevo = malloc(sizeof(t_mutex_kernel));

    nuevo->nombre = strdup(nombre);
    nuevo->duenio = NULL;
    nuevo->bloqueados = queue_create();

    pthread_mutex_init(&nuevo->mutex_interno, NULL);

    list_add(lista_mutexes, nuevo);

    return nuevo;
}

//Busca Mutex

t_mutex_kernel* buscar_mutex(char* nombre) {

    for(int i = 0; i < list_size(lista_mutexes); i++) {

        t_mutex_kernel* mutex = list_get(lista_mutexes, i);

        if(strcmp(mutex->nombre, nombre) == 0) {
            return mutex;
        }
    }

    return NULL;
}

//Lock

bool mutex_lock(char* nombre, t_pcb* proceso) {

    t_mutex_kernel* mutex = buscar_mutex(nombre);

    if(mutex == NULL) {
        return false;
    }

    pthread_mutex_lock(&mutex->mutex_interno);

    //Mutex libre
    if(mutex->duenio == NULL) {

        mutex->duenio = proceso;

        pthread_mutex_unlock(&mutex->mutex_interno);

        return true;
    }

    //Mutex ocupado
    queue_push(mutex->bloqueados, proceso);

    pthread_mutex_unlock(&mutex->mutex_interno);

    quitar_de_exec(proceso->pid);

    cambiar_estado(proceso, ESTADO_BLOCK, logger);

    agregar_a_block(proceso);

    return false;
}

//Unlock

void mutex_unlock(char* nombre) {

    t_mutex_kernel* mutex = buscar_mutex(nombre);

    if(mutex == NULL) {
        return;
    }

    pthread_mutex_lock(&mutex->mutex_interno);

    //No hay bloqueados
    if(queue_is_empty(mutex->bloqueados)) {

        mutex->duenio = NULL;

        pthread_mutex_unlock(&mutex->mutex_interno);

        return;
    }

    //Hay bloqueados
    t_pcb* siguiente = queue_pop(mutex->bloqueados);

    mutex->duenio = siguiente;

    pthread_mutex_unlock(&mutex->mutex_interno);

    cambiar_estado(siguiente, ESTADO_READY, logger);

    agregar_a_ready(siguiente);
}

//Handler Lock

void manejar_mutex_lock(int socket_cpu, t_pcb* proceso) {

    char nombre_mutex[64];

    recibir_string(socket_cpu, nombre_mutex, sizeof(nombre_mutex));

    bool conseguido = mutex_lock(nombre_mutex, proceso);

    if(conseguido) {

        log_info(logger,
            "## (%d) obtuvo mutex %s",
            proceso->pid,
            nombre_mutex
        );
    }
    else {

        log_info(logger,
            "## (%d) bloqueado esperando mutex %s",
            proceso->pid,
            nombre_mutex
        );
        cancelar_timer(socket_cpu);
        agregar_cpu_libre(socket_cpu);
    }
}

//Handler Unlock

void manejar_mutex_unlock(int socket_cpu, t_pcb* proceso) {

    char nombre_mutex[64];

    recibir_string(socket_cpu, nombre_mutex, sizeof(nombre_mutex));

    mutex_unlock(nombre_mutex);

    log_info(logger,
        "## (%d) liberó mutex %s",
        proceso->pid,
        nombre_mutex
    );
}
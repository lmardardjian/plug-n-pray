#include "procesos.h"
#include "scheduler.h"
#include "planificador.h"
#include "mutex_manager.h"
#include "utils/conexion.h"
#include "utils/constantes.h"
#include <string.h>

extern t_log* logger;
t_list* lista_mutexes;
static pthread_mutex_t mutex_lista_mutexes;

void inicializar_ks_mutex_manager() {
    lista_mutexes = list_create();
    pthread_mutex_init(&mutex_lista_mutexes, NULL);
}

//Crea Mutex

t_mutex_kernel* crear_mutex(char* nombre) {

    t_mutex_kernel* existente = buscar_mutex(nombre);
    if (existente != NULL) {
        log_warning(logger, "MUTEX_CREATE: el mutex '%s' ya existe, se ignora.", nombre);
        return existente;
    }

    t_mutex_kernel* nuevo = malloc(sizeof(t_mutex_kernel));

    nuevo->nombre = strdup(nombre);
    nuevo->duenio = NULL;
    nuevo->bloqueados = list_create();

    pthread_mutex_init(&nuevo->mutex_interno, NULL);

    pthread_mutex_lock(&mutex_lista_mutexes);

    list_add(lista_mutexes, nuevo);

    pthread_mutex_unlock(&mutex_lista_mutexes);

    return nuevo;
}

//Busca Mutex

t_mutex_kernel* buscar_mutex(char* nombre) {

    pthread_mutex_lock(&mutex_lista_mutexes);

    t_mutex_kernel* resultado = NULL;
    for(int i = 0; i < list_size(lista_mutexes); i++) {
        t_mutex_kernel* mutex = list_get(lista_mutexes, i);
        if(strcmp(mutex->nombre, nombre) == 0) {
            resultado = mutex;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_lista_mutexes);
    
    return resultado;
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
    list_add(mutex->bloqueados, proceso);

    // Si el proceso bloqueado tiene mayor prioridad que el dueño (número menor = mayor prioridad)
    if (proceso->prioridad < mutex->duenio->prioridad) {
        uint32_t prioridad_nueva = proceso->prioridad;
        t_pcb* duenio = mutex->duenio;
    
        log_info(logger, "## %d Cambio de prioridad: %d - %d", duenio->pid, duenio->prioridad, prioridad_nueva);
        duenio->prioridad = prioridad_nueva;
    
        // Si el dueño está en READY, reposicionarlo en la cola correcta
        if (duenio->estado == ESTADO_READY) {
            t_pcb* p = quitar_de_ready_por_pid(duenio->pid);
            if (p != NULL)
                reinsertar_al_principio_de_ready(p);
        }
    }

    pthread_mutex_unlock(&mutex->mutex_interno);

    quitar_de_exec(proceso->pid);

    cambiar_estado(proceso, ESTADO_BLOCK, logger);

    agregar_a_block(proceso);

    return false;
}

// Recorre TODOS los mutexes del sistema y, de los que 'proceso' sigue
// teniendo tomados, calcula cuál es la prioridad más alta (número más bajo)
// entre los procesos que están esperando alguno de ellos. Si no hay ningún
// mutex propio con gente esperando, devuelve la prioridad_original: recién
// ahí corresponde restaurarla.
//
// Esto reemplaza la lógica anterior, que sólo miraba si HABÍA gente
// esperando el mutex puntual que se estaba liberando, ignorando que el
// proceso podía seguir siendo dueño de otro mutex que sí justificaba
// mantener la prioridad heredada.
static uint32_t calcular_prioridad_necesaria(t_pcb* proceso) {
 
    uint32_t prioridad_minima = proceso->prioridad_original;
 
    pthread_mutex_lock(&mutex_lista_mutexes);
 
    int cantidad_mutexes = list_size(lista_mutexes);
    for (int i = 0; i < cantidad_mutexes; i++) {
        t_mutex_kernel* mutex = list_get(lista_mutexes, i);
 
        pthread_mutex_lock(&mutex->mutex_interno);
 
        if (mutex->duenio == proceso) {
            int cantidad_bloqueados = list_size(mutex->bloqueados);
            for (int j = 0; j < cantidad_bloqueados; j++) {
                t_pcb* esperando = list_get(mutex->bloqueados, j);
                if (esperando->prioridad < prioridad_minima)
                    prioridad_minima = esperando->prioridad;
            }
        }
 
        pthread_mutex_unlock(&mutex->mutex_interno);
    }
 
    pthread_mutex_unlock(&mutex_lista_mutexes);
 
    return prioridad_minima;
}

//Unlock

void mutex_unlock(char* nombre) {

    t_mutex_kernel* mutex = buscar_mutex(nombre);

    if(mutex == NULL) {
        return;//falta log error
    }

    pthread_mutex_lock(&mutex->mutex_interno);

    t_pcb* duenio_anterior = mutex->duenio;

    if (duenio_anterior == NULL) {
        pthread_mutex_unlock(&mutex->mutex_interno);
        log_warning(logger, "MUTEX_UNLOCK: mutex '%s' no tenía dueño.", nombre);
        return;
    }

    //No hay bloqueados
    if(list_is_empty(mutex->bloqueados)) {

        mutex->duenio = NULL;

        pthread_mutex_unlock(&mutex->mutex_interno);

        /*if (duenio_anterior->prioridad != duenio_anterior->prioridad_original) {
            log_info(logger, "## %d Cambio de prioridad: %d - %d", duenio_anterior->pid, duenio_anterior->prioridad, duenio_anterior->prioridad_original);
            duenio_anterior->prioridad = duenio_anterior->prioridad_original;
        }*/
        
    } else {

        //Hay bloqueados: se lo doy al primero que llegó (FIFO), según lo pedido por la consigna.
        t_pcb* siguiente = list_remove(mutex->bloqueados, 0);
        mutex->duenio = siguiente;

        // Entre los que quedan esperando este mismo mutex puede haber alguno
        // de mayor prioridad que 'siguiente' (el FIFO no garantiza que el
        // primero en pedirlo sea el más prioritario). Si no chequeamos esto
        // acá, 'siguiente' se queda con su prioridad original hasta que
        // llegue algún MUTEX_LOCK nuevo que dispare el chequeo — dejando una
        // ventana de inversión de prioridades con quien ya estaba esperando
        // desde antes.
        int cantidad_restantes = list_size(mutex->bloqueados);
            for (int i = 0; i < cantidad_restantes; i++) {
             t_pcb* esperando = list_get(mutex->bloqueados, i);
                if (esperando->prioridad < siguiente->prioridad) {
                    log_info(logger, "## %d Cambio de prioridad: %d - %d", siguiente->pid, siguiente->prioridad, esperando->prioridad);
                    siguiente->prioridad = esperando->prioridad;
            }
        }

        pthread_mutex_unlock(&mutex->mutex_interno);

        cambiar_estado(siguiente, ESTADO_READY, logger);
        agregar_a_ready(siguiente);
    }

    // Ya soltamos este mutex (mutex->duenio ya no apunta a duenio_anterior),
    // así que ahora sí podemos preguntar con seguridad, mirando el resto de
    // los mutexes del sistema, si el ex-dueño TODAVÍA necesita la prioridad
    // heredada por seguir siendo dueño de algún otro mutex con gente esperando.
    uint32_t prioridad_correcta = calcular_prioridad_necesaria(duenio_anterior);

    if (duenio_anterior->prioridad != prioridad_correcta) {
        log_info(logger, "## %d Cambio de prioridad: %d - %d", duenio_anterior->pid, duenio_anterior->prioridad, duenio_anterior->prioridad_correcta);
        duenio_anterior->prioridad = duenio_anterior->prioridad_correcta;
    }

}

//Handler Lock

void  manejar_syscall_mutex_lock(int socket_cpu, t_pcb* proceso) {

    char nombre_mutex[MAX_NOMBRE_MUTEX];

    uint32_t tipo_inst;
    char param2[32] = {0};
    recibir_uint32(socket_cpu, &tipo_inst);
    recibir_string(socket_cpu, nombre_mutex, sizeof(nombre_mutex));
    recibir_string(socket_cpu, param2, sizeof(param2));

    log_info(logger, "## (%d) - Solicitó syscall: MUTEX_LOCK", proceso->pid);

    bool conseguido = mutex_lock(nombre_mutex, proceso);

    if(conseguido) {
        log_info(logger, "## (%d) Toma el Mutex %s", proceso->pid, nombre_mutex);
        //el proceso sigue corriendo en la misma CPU: recrear el timer y reenviarle el PID para que ciclo_instruccion continue.
        cancelar_timer(socket_cpu);
        recrear_timer(socket_cpu, proceso);
        enviar_uint32(socket_cpu, proceso->pid);
    }
    else {
        log_info(logger, "## (%d) bloqueado esperando mutex %s", proceso->pid,nombre_mutex);
        cancelar_timer(socket_cpu);
        agregar_cpu_libre(socket_cpu);
    }
}

//Handler Unlock

void manejar_syscall_mutex_unlock (int socket_cpu, t_pcb* proceso) {

    char nombre_mutex[MAX_NOMBRE_MUTEX];

    uint32_t tipo_inst;
    char param2[32] = {0};
    recibir_uint32(socket_cpu, &tipo_inst);
    recibir_string(socket_cpu, nombre_mutex, sizeof(nombre_mutex));
    recibir_string(socket_cpu, param2, sizeof(param2));

    log_info(logger, "## (%d) - Solicitó syscall: MUTEX_UNLOCK", proceso->pid);

    cancelar_timer(socket_cpu);
    quitar_de_exec(proceso->pid);

    mutex_unlock(nombre_mutex);

    log_info(logger, "## (%d) Libera el Mutex %s", proceso->pid, nombre_mutex);

    cambiar_estado(proceso, ESTADO_READY, logger);
    agregar_a_ready(proceso);
    agregar_cpu_libre(socket_cpu);

}
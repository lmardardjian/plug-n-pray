#include "utils/hilos.h"
#include <pthread.h>

void crear_hilo(void* (*funcion)(void*), void* arg)
{
    pthread_t hilo;

    pthread_create(&hilo, NULL, funcion, arg);

    pthread_detach(hilo);
}
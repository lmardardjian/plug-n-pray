#ifndef UTILS_HILOS_H
#define UTILS_HILOS_H

#include <stdlib.h>
#include <stdio.h>

void crear_hilo(void* (*funcion)(void*), void* arg);

#endif

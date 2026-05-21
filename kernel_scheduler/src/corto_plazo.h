#ifndef CORTO_PLAZO_H
#define CORTO_PLAZO_H

#include "pcb.h"
#include "utils/mensajes.h"
#include <pthread.h>
#include <semaphore.h>
#include <commons/collections/queue.h>

void inicializar_corto_plazo();
void agregar_cpu_libre(int socket_cpu);
void* hilo_dispatcher(void* arg);
void manejar_syscall_io(int socket_cpu, t_pcb* proceso, op_code tipo_io);
void manejar_exit(int socket_cpu, t_pcb* proceso);
void manejar_fin_quantum(int socket_cpu, t_pcb* proceso);

#endif
#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include "utils/mensajes.h"
#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

// globales expuestos
extern t_log*    logger;
extern t_config* config;


// hueco libre en la memoria física
typedef struct {
    uint32_t base;
    uint32_t tamanio;
} t_hueco;

// segmento guardado en SWAP (mientras el proceso esta suspendido)
typedef struct {
    uint32_t id_segmento;
    uint32_t tamanio;     // tamanio real del segmento (puede no ser multiplo de block_size)
    t_list*  bloques;     // lista de uint32_t*, numeros de bloque de SWAP, en orden
} t_segmento_swap;

// proceso en memoria (instrucciones + contexto de ejecucion)
typedef struct {
    uint32_t  pid;
    t_list*   instrucciones;   
    t_contexto contexto;       // incluye tabla_segmentos 
    t_list* segmentos_suspendidos;      // lista de t_segmento_swap* guardados al suspender
} t_proceso_memoria;

// estado global

extern t_list*       g_memory_sticks;
extern pthread_mutex_t g_mutex_sticks;

extern t_dictionary* g_procesos;
extern pthread_mutex_t g_mutex_procesos;

extern t_list*       g_huecos;          // lista de t_hueco*, ordenada por base
extern pthread_mutex_t g_mutex_huecos;

extern int g_socket_ks_operaciones;
extern int g_socket_ks_notificaciones;
extern pthread_mutex_t g_mutex_ks_notif;

extern uint32_t g_segment_max_size;
extern char     g_allocation_strategy[8]; // "BEST" o "WORST"

// SWAP
extern int             g_socket_swap;        // -1 si no hay swap conectado
extern pthread_mutex_t g_mutex_swap;         // serializa el uso del socket de swap
extern uint32_t         g_swap_block_size;
extern uint32_t         g_swap_total_bloques;
extern t_list*          g_bloques_libres;    // lista de uint32_t*, numeros de bloque libres
extern pthread_mutex_t  g_mutex_bloques_swap;

// arg para hilos
typedef struct {
    int socket;
} t_args_cliente;

void inicializar_estado_global(t_config* cfg);
void inicializar_contexto(t_contexto* ctx);
t_list* leer_instrucciones(const char* path);

// gestión de huecos
int      seleccionar_hueco(uint32_t tamanio);
uint32_t ocupar_hueco(int indice, uint32_t tamanio);
void     liberar_segmento(uint32_t base, uint32_t tamanio);
uint32_t total_libre(void);

// lectura / escritura en sticks
void* leer_de_sticks(uint32_t dir_fisica, uint32_t tamanio);
int   escribir_en_sticks(uint32_t dir_fisica, void* datos, uint32_t tamanio);

// compactación
void compactar_memoria(void);

// swap: manejo de bloques
t_list* asignar_bloques_swap(uint32_t cantidad);   // NULL si no hay suficientes
void    liberar_bloques_swap(t_list* bloques);     // devuelve los bloques al pool y destruye la lista
int     leer_bloque_swap(uint32_t num_bloque, void* destino);
int     escribir_bloque_swap(uint32_t num_bloque, void* datos);

// notificaciones al kernel_scheduler
void notificar_bsod_al_scheduler(void);
void notificar_memoria_libre_al_scheduler(void);

// handlers de operaciones
void op_crear_proceso(int cliente);
void op_enviar_instruccion(int cliente);
void op_enviar_contexto(int cliente);
void op_actualizar_contexto(int cliente);
void op_mem_alloc(int cliente);
void op_mem_free(int cliente);
void op_mem_read(int cliente);
void op_mem_write(int cliente);
void op_finalizar_proceso(int cliente);
void op_suspender_proceso(int cliente);
void op_reanudar_proceso(int cliente);

// hilo de atención
void* atender_cliente(void* arg);

#endif
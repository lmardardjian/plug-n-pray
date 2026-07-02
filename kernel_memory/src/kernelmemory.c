#include "kernelmemory.h"
#include "utils/conexion.h"
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <semaphore.h>

// VARIABLES GLOBALES
t_list*         g_memory_sticks  = NULL;
pthread_mutex_t g_mutex_sticks   = PTHREAD_MUTEX_INITIALIZER;

t_dictionary*   g_procesos       = NULL;
pthread_mutex_t g_mutex_procesos = PTHREAD_MUTEX_INITIALIZER;

t_list*         g_huecos         = NULL;
pthread_mutex_t g_mutex_huecos   = PTHREAD_MUTEX_INITIALIZER;

int g_socket_ks_operaciones    = -1;
int g_socket_ks_notificaciones = -1;
pthread_mutex_t g_mutex_ks_notif = PTHREAD_MUTEX_INITIALIZER;

static sem_t sem_compactacion_ok;

uint32_t g_segment_max_size     = 256;
char     g_allocation_strategy[8] = "BEST";

int             g_socket_swap        = -1;
pthread_mutex_t g_mutex_swap         = PTHREAD_MUTEX_INITIALIZER;
uint32_t        g_swap_block_size    = 0;
uint32_t        g_swap_total_bloques = 0;
t_list*         g_bloques_libres     = NULL;
pthread_mutex_t g_mutex_bloques_swap = PTHREAD_MUTEX_INITIALIZER;

// inicializacion
void inicializar_estado_global(t_config* cfg)
{
    g_memory_sticks = list_create();
    g_procesos      = dictionary_create();
    g_huecos        = list_create();

    g_segment_max_size = (uint32_t)config_get_int_value(cfg, "SEGMENT_MAX_SIZE");

    char* strat = config_get_string_value(cfg, "ALLOCATION_STRATEGY");
    strncpy(g_allocation_strategy, strat, sizeof(g_allocation_strategy) - 1);
    g_allocation_strategy[sizeof(g_allocation_strategy) - 1] = '\0';

    sem_init(&sem_compactacion_ok, 0, 0);
    
}

// contexto
void inicializar_contexto(t_contexto* ctx)
{
    ctx->pc  = 0; ctx->ax  = 0; ctx->bx  = 0;
    ctx->cx  = 0; ctx->dx  = 0; ctx->eax = 0;
    ctx->ebx = 0; ctx->ecx = 0; ctx->edx = 0;
    ctx->si  = 0; ctx->di  = 0;
    ctx->tabla_segmentos = list_create();
}

// instrucciones
t_list* leer_instrucciones(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    t_list* lista = list_create();
    char linea[512];
    while (fgets(linea, sizeof(linea), f)) {
        linea[strcspn(linea, "\n")] = '\0';
        list_add(lista, strdup(linea));
    }
    fclose(f);
    return lista;
}

// gestion de huecos
// comparador para list_sort: ordena huecos por base ascendente
static bool cmp_huecos_base(void* a, void* b)
{
    return ((t_hueco*)a)->base < ((t_hueco*)b)->base;
}

// fusiona huecos adyacentes. Llamar con g_mutex_huecos tomado.
static void fusionar_huecos(void)
{
    list_sort(g_huecos, cmp_huecos_base);

    for (int i = 0; i < (int)list_size(g_huecos) - 1; ) {
        t_hueco* a = list_get(g_huecos, i);
        t_hueco* b = list_get(g_huecos, i + 1);
        if (a->base + a->tamanio == b->base) {
            a->tamanio += b->tamanio;
            list_remove(g_huecos, i + 1);
            free(b);
        } else {
            i++;
        }
    }
}

// selecciona el índice del hueco a usar según la estrategia.
// debe llamarse con g_mutex_huecos tomado.
// devuelve -1 si no hay hueco suficiente.
int seleccionar_hueco(uint32_t tamanio)
{
    int      elegido  = -1;
    uint32_t mejor    = 0;
    bool     es_best  = (strcmp(g_allocation_strategy, "BEST") == 0);

    for (int i = 0; i < (int)list_size(g_huecos); i++) {
        t_hueco* h = list_get(g_huecos, i);
        if (h->tamanio < tamanio) continue;

        if (elegido == -1) {
            elegido = i;
            mejor   = h->tamanio;
            continue;
        }
        if (es_best && h->tamanio < mejor) { elegido = i; mejor = h->tamanio; }
        if (!es_best && h->tamanio > mejor) { elegido = i; mejor = h->tamanio; }
    }
    return elegido;
}

// ocupa 'tamanio' bytes del hueco en 'indice'.
// debe llamarse con g_mutex_huecos tomado.
// devuelve la dirección física asignada.
uint32_t ocupar_hueco(int indice, uint32_t tamanio)
{
    t_hueco* h = list_get(g_huecos, indice);
    uint32_t base = h->base;

    if (h->tamanio == tamanio) {
        list_remove(g_huecos, indice);
        free(h);
    } else {
        h->base    += tamanio;
        h->tamanio -= tamanio;
    }
    return base;
}

// libera un segmento y fusiona huecos.
void liberar_segmento(uint32_t base, uint32_t tamanio)
{
    pthread_mutex_lock(&g_mutex_huecos);
    t_hueco* h = malloc(sizeof(t_hueco));
    h->base    = base;
    h->tamanio = tamanio;
    list_add(g_huecos, h);
    fusionar_huecos();
    pthread_mutex_unlock(&g_mutex_huecos);
}

// suma de todos los huecos.
uint32_t total_libre(void)
{
    pthread_mutex_lock(&g_mutex_huecos);
    uint32_t total = 0;
    for (int i = 0; i < (int)list_size(g_huecos); i++)
        total += ((t_hueco*)list_get(g_huecos, i))->tamanio;
    pthread_mutex_unlock(&g_mutex_huecos);
    return total;
}

// versión con lock (uso normal desde handlers)
void* leer_de_sticks(uint32_t dir_fisica, uint32_t tamanio)
{
    void*    resultado = malloc(tamanio);
    uint32_t leido     = 0;
    uint32_t dir_act   = dir_fisica;

    pthread_mutex_lock(&g_mutex_sticks);
    while (leido < tamanio) {
        // encontrar el stick que contiene dir_act
        t_memory_stick* s = NULL;
        for (int i = 0; i < (int)list_size(g_memory_sticks); i++) {
            t_memory_stick* si = list_get(g_memory_sticks, i);
            if (dir_act >= si->base_fisica &&
                dir_act <  si->base_fisica + si->tamanio) { s = si; break; }
        }
        if (!s) { pthread_mutex_unlock(&g_mutex_sticks); free(resultado); return NULL; }

        uint32_t off  = dir_act - s->base_fisica;
        uint32_t disp = s->tamanio - off;
        uint32_t lr   = (tamanio - leido < disp) ? (tamanio - leido) : disp;

        enviar_opcode(s->socket, MS_LEER);
        enviar_uint32(s->socket, off);
        enviar_uint32(s->socket, lr);

        op_code resp;
        recibir_opcode(s->socket, &resp);
        if (resp == RESPUESTA_ERROR) {
            pthread_mutex_unlock(&g_mutex_sticks); free(resultado); return NULL;
        }
        recibir_buffer(s->socket, (char*)resultado + leido, lr);

        leido   += lr;
        dir_act += lr;
    }
    pthread_mutex_unlock(&g_mutex_sticks);
    return resultado;
}

int escribir_en_sticks(uint32_t dir_fisica, void* datos, uint32_t tamanio)
{
    uint32_t escrito = 0;
    uint32_t dir_act = dir_fisica;

    pthread_mutex_lock(&g_mutex_sticks);
    while (escrito < tamanio) {
        t_memory_stick* s = NULL;
        for (int i = 0; i < (int)list_size(g_memory_sticks); i++) {
            t_memory_stick* si = list_get(g_memory_sticks, i);
            if (dir_act >= si->base_fisica &&
                dir_act <  si->base_fisica + si->tamanio) { s = si; break; }
        }
        if (!s) { pthread_mutex_unlock(&g_mutex_sticks); return -1; }

        uint32_t off = dir_act - s->base_fisica;
        uint32_t disp = s->tamanio - off;
        uint32_t lw   = (tamanio - escrito < disp) ? (tamanio - escrito) : disp;

        enviar_opcode(s->socket, MS_ESCRIBIR);
        enviar_uint32(s->socket, off);
        enviar_uint32(s->socket, lw);
        enviar_buffer(s->socket, (char*)datos + escrito, lw);

        op_code resp;
        recibir_opcode(s->socket, &resp);
        if (resp == RESPUESTA_ERROR) { pthread_mutex_unlock(&g_mutex_sticks); return -1; }

        escrito += lw;
        dir_act += lw;
    }
    pthread_mutex_unlock(&g_mutex_sticks);
    return 0;
}

// versiones sin lock para usar dentro de compactar_memoria
// (que ya tiene los 3 locks tomados)
static void* leer_de_sticks_sin_lock(uint32_t dir_fisica, uint32_t tamanio)
{
    void*    resultado = malloc(tamanio);
    uint32_t leido     = 0;
    uint32_t dir_act   = dir_fisica;

    while (leido < tamanio) {
        t_memory_stick* s = NULL;
        for (int i = 0; i < (int)list_size(g_memory_sticks); i++) {
            t_memory_stick* si = list_get(g_memory_sticks, i);
            if (dir_act >= si->base_fisica &&
                dir_act <  si->base_fisica + si->tamanio) { s = si; break; }
        }
        if (!s) { free(resultado); return NULL; }

        uint32_t off  = dir_act - s->base_fisica;
        uint32_t disp = s->tamanio - off;
        uint32_t lr   = (tamanio - leido < disp) ? (tamanio - leido) : disp;

        enviar_opcode(s->socket, MS_LEER);
        enviar_uint32(s->socket, off);
        enviar_uint32(s->socket, lr);

        op_code resp;
        recibir_opcode(s->socket, &resp);
        if (resp == RESPUESTA_ERROR) { free(resultado); return NULL; }
        recibir_buffer(s->socket, (char*)resultado + leido, lr);

        leido   += lr;
        dir_act += lr;
    }
    return resultado;
}

static int escribir_en_sticks_sin_lock(uint32_t dir_fisica, void* datos, uint32_t tamanio)
{
    uint32_t escrito = 0;
    uint32_t dir_act = dir_fisica;

    while (escrito < tamanio) {
        t_memory_stick* s = NULL;
        for (int i = 0; i < (int)list_size(g_memory_sticks); i++) {
            t_memory_stick* si = list_get(g_memory_sticks, i);
            if (dir_act >= si->base_fisica &&
                dir_act <  si->base_fisica + si->tamanio) { s = si; break; }
        }
        if (!s) return -1;

        uint32_t off  = dir_act - s->base_fisica;
        uint32_t disp = s->tamanio - off;
        uint32_t lw   = (tamanio - escrito < disp) ? (tamanio - escrito) : disp;

        enviar_opcode(s->socket, MS_ESCRIBIR);
        enviar_uint32(s->socket, off);
        enviar_uint32(s->socket, lw);
        enviar_buffer(s->socket, (char*)datos + escrito, lw);

        op_code resp;
        recibir_opcode(s->socket, &resp);
        if (resp == RESPUESTA_ERROR) return -1;

        escrito += lw;
        dir_act += lw;
    }
    return 0;
}

// compactacion
// acumulador de segmentos para la iteración del diccionario
static t_list* g_segs_compactar = NULL;

static void recolectar_segmentos(char* key, void* val)
{
    (void)key;
    t_proceso_memoria* proc = (t_proceso_memoria*)val;
    for (int i = 0; i < (int)list_size(proc->contexto.tabla_segmentos); i++)
        list_add(g_segs_compactar, list_get(proc->contexto.tabla_segmentos, i));
}

static bool cmp_segs_base(void* a, void* b)
{
    return ((t_segmento*)a)->base < ((t_segmento*)b)->base;
}

void compactar_memoria(void)
{
    log_info(logger, "## Inicio de compactación");

    int delay_ms = config_get_int_value(config, "COMPACTION_DELAY");
    usleep((useconds_t)delay_ms * 1000);

    pthread_mutex_lock(&g_mutex_procesos);
    pthread_mutex_lock(&g_mutex_huecos);
    pthread_mutex_lock(&g_mutex_sticks);

    g_segs_compactar = list_create();
    dictionary_iterator(g_procesos, recolectar_segmentos);

    list_sort(g_segs_compactar, cmp_segs_base);

    uint32_t offset = 0;
    for (int i = 0; i < (int)list_size(g_segs_compactar); i++) {
        t_segmento* seg = list_get(g_segs_compactar, i);
        if (seg->base != offset) {
            void* datos = leer_de_sticks_sin_lock(seg->base, seg->limite);
            if (datos) {
                escribir_en_sticks_sin_lock(offset, datos, seg->limite);
                free(datos);
            }
            seg->base = offset;   // actualiza el puntero real en la tabla del proceso
        }
        offset += seg->limite;
    }
    list_destroy(g_segs_compactar);
    g_segs_compactar = NULL;

    list_destroy_and_destroy_elements(g_huecos, free);
    g_huecos = list_create();

    uint32_t total_mem = 0;
    for (int i = 0; i < (int)list_size(g_memory_sticks); i++)
        total_mem += ((t_memory_stick*)list_get(g_memory_sticks, i))->tamanio;

    if (offset < total_mem) {
        t_hueco* h = malloc(sizeof(t_hueco));
        h->base    = offset;
        h->tamanio = total_mem - offset;
        list_add(g_huecos, h);
    }

    pthread_mutex_unlock(&g_mutex_sticks);
    pthread_mutex_unlock(&g_mutex_huecos);
    pthread_mutex_unlock(&g_mutex_procesos);

    log_info(logger, "## Fin de compactación");

    notificar_memoria_libre_al_scheduler();
}

// swap: manejo de bloques

// pide 'cantidad' bloques libres. Devuelve NULL si no hay suficientes.
t_list* asignar_bloques_swap(uint32_t cantidad)
{
    t_list* asignados = list_create();

    pthread_mutex_lock(&g_mutex_bloques_swap);
    if (!g_bloques_libres || (uint32_t)list_size(g_bloques_libres) < cantidad) {
        pthread_mutex_unlock(&g_mutex_bloques_swap);
        list_destroy(asignados);
        return NULL;
    }
    for (uint32_t i = 0; i < cantidad; i++)
        list_add(asignados, list_remove(g_bloques_libres, 0));
    pthread_mutex_unlock(&g_mutex_bloques_swap);

    return asignados;
}

// devuelve los bloques al pool de libres y destruye la lista recibida
// (no libera los uint32_t*, pasan a ser propiedad de g_bloques_libres)
void liberar_bloques_swap(t_list* bloques)
{
    if (!bloques) return;

    pthread_mutex_lock(&g_mutex_bloques_swap);
    for (int i = 0; i < (int)list_size(bloques); i++)
        list_add(g_bloques_libres, list_get(bloques, i));
    pthread_mutex_unlock(&g_mutex_bloques_swap);

    list_destroy(bloques);
}

// lee un bloque completo (g_swap_block_size bytes) desde el modulo SWAP
int leer_bloque_swap(uint32_t num_bloque, void* destino)
{
    if (g_socket_swap == -1) return -1;

    pthread_mutex_lock(&g_mutex_swap);
    enviar_opcode(g_socket_swap, SW_LEER);
    enviar_uint32(g_socket_swap, num_bloque);

    op_code resp;
    if (recibir_opcode(g_socket_swap, &resp) <= 0 || resp != RESPUESTA_OK) {
        pthread_mutex_unlock(&g_mutex_swap);
        return -1;
    }
    recibir_buffer(g_socket_swap, destino, g_swap_block_size);
    pthread_mutex_unlock(&g_mutex_swap);
    return 0;
}

// escribe un bloque completo (g_swap_block_size bytes) en el modulo SWAP
int escribir_bloque_swap(uint32_t num_bloque, void* datos)
{
    if (g_socket_swap == -1) return -1;

    pthread_mutex_lock(&g_mutex_swap);
    enviar_opcode(g_socket_swap, SW_ESCRIBIR);
    enviar_uint32(g_socket_swap, num_bloque);
    enviar_buffer(g_socket_swap, datos, g_swap_block_size);

    op_code resp;
    if (recibir_opcode(g_socket_swap, &resp) <= 0 || resp != RESPUESTA_OK) {
        pthread_mutex_unlock(&g_mutex_swap);
        return -1;
    }
    pthread_mutex_unlock(&g_mutex_swap);
    return 0;
}

// libera un t_segmento_swap: devuelve sus bloques al pool y lo destruye
static void destruir_segmento_swap(void* elem)
{
    t_segmento_swap* ss = (t_segmento_swap*)elem;
    liberar_bloques_swap(ss->bloques);
    free(ss);
}

// notificaciones al kernel scheduler
void notificar_bsod_al_scheduler(void)
{
    pthread_mutex_lock(&g_mutex_ks_notif);
    if (g_socket_ks_notificaciones != -1) {
        log_error(logger, "## Notificando BSOD al Kernel Scheduler");
        enviar_opcode(g_socket_ks_notificaciones, KM_BSOD);
    }
    pthread_mutex_unlock(&g_mutex_ks_notif);
}

void notificar_memoria_libre_al_scheduler(void)
{
    pthread_mutex_lock(&g_mutex_ks_notif);
    if (g_socket_ks_notificaciones != -1) {
        log_info(logger, "## Notificando memoria libre al Kernel Scheduler");
        enviar_opcode(g_socket_ks_notificaciones, KM_NOTIF_MEMORIA_LIBRE);
    }
    pthread_mutex_unlock(&g_mutex_ks_notif);
}

// handlers de operaciones
void op_crear_proceso(int cliente)
{
    uint32_t pid;
    recibir_uint32(cliente, &pid);

    char path_relativo[512];
    recibir_string(cliente, path_relativo, sizeof(path_relativo));

    char* basepath = config_get_string_value(config, "SCRIPTS_BASEPATH");
    char  path_completo[1024];
    snprintf(path_completo, sizeof(path_completo), "%s/%s", basepath, path_relativo);

    t_proceso_memoria* proc = malloc(sizeof(t_proceso_memoria));
    proc->pid = pid;
    inicializar_contexto(&proc->contexto);
    proc->segmentos_suspendidos = list_create();
    proc->instrucciones = leer_instrucciones(path_completo);

    if (!proc->instrucciones) {
        log_error(logger, "## PID: %u - No se pudo leer: %s", pid, path_completo);
        free(proc);
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }

    char key[20];
    snprintf(key, sizeof(key), "%u", pid);

    pthread_mutex_lock(&g_mutex_procesos);
    dictionary_put(g_procesos, key, proc);
    pthread_mutex_unlock(&g_mutex_procesos);

    log_info(logger, "## PID: %u - Proceso Creado", pid);
    enviar_opcode(cliente, RESPUESTA_OK);
}

void op_enviar_instruccion(int cliente)
{
    uint32_t pid, pc;
    recibir_uint32(cliente, &pid);
    recibir_uint32(cliente, &pc);

    int delay_ms = config_get_int_value(config, "INSTRUCTION_DELAY");
    usleep((useconds_t)delay_ms * 1000);

    char key[20];
    snprintf(key, sizeof(key), "%u", pid);

    pthread_mutex_lock(&g_mutex_procesos);
    t_proceso_memoria* proc = dictionary_get(g_procesos, key);
    if (!proc || pc >= (uint32_t)list_size(proc->instrucciones)) {
        pthread_mutex_unlock(&g_mutex_procesos);
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }
    char* instruccion = strdup(list_get(proc->instrucciones, pc));
    pthread_mutex_unlock(&g_mutex_procesos);

    log_info(logger, "## PID: %u - Obtener instrucción: %u - Instrucción: %s", pid, pc, instruccion);

    enviar_opcode(cliente, RESPUESTA_OK);
    enviar_string(cliente, instruccion);
    free(instruccion);
}

void op_enviar_contexto(int cliente)
{
    uint32_t pid;
    recibir_uint32(cliente, &pid);

    char key[20];
    snprintf(key, sizeof(key), "%u", pid);

    pthread_mutex_lock(&g_mutex_procesos);
    t_proceso_memoria* proc = dictionary_get(g_procesos, key);
    if (!proc) {
        pthread_mutex_unlock(&g_mutex_procesos);
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }
    enviar_opcode(cliente, RESPUESTA_OK);
    enviar_contexto_serializado(cliente, &proc->contexto);
    pthread_mutex_unlock(&g_mutex_procesos);

    log_info(logger, "## Contexto enviado PID %u", pid);
}

void op_actualizar_contexto(int cliente)
{
    uint32_t pid;
    recibir_uint32(cliente, &pid);

    char key[20];
    snprintf(key, sizeof(key), "%u", pid);

    pthread_mutex_lock(&g_mutex_procesos);
    t_proceso_memoria* proc = dictionary_get(g_procesos, key);
    if (!proc) {
        pthread_mutex_unlock(&g_mutex_procesos);
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }

    recibir_contexto_serializado(cliente, &proc->contexto);
    pthread_mutex_unlock(&g_mutex_procesos);

    log_info(logger, "## Contexto actualizado PID %u", pid);
    enviar_opcode(cliente, RESPUESTA_OK);
}

void op_mem_alloc(int cliente)
{
    uint32_t pid, id_segmento, tamanio;
    recibir_uint32(cliente, &pid);
    recibir_uint32(cliente, &id_segmento);
    recibir_uint32(cliente, &tamanio);

    if (tamanio > g_segment_max_size) {
        log_error(logger, "## PID: %u - Segmento %u supera SEGMENT_MAX_SIZE (%u)",
                  pid, id_segmento, g_segment_max_size);
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }

    pthread_mutex_lock(&g_mutex_huecos);
    int idx = seleccionar_hueco(tamanio);

    if (idx == -1) {
        uint32_t libre = 0;
        for (int i = 0; i < (int)list_size(g_huecos); i++)
            libre += ((t_hueco*)list_get(g_huecos, i))->tamanio;
        pthread_mutex_unlock(&g_mutex_huecos);

        if (libre < tamanio) {
            log_error(logger, "## PID: %u - Sin memoria suficiente", pid);
            enviar_opcode(cliente, RESPUESTA_ERROR);
            return;
        }

        // Hay espacio total pero no contiguo → compactar
        log_info(logger, "## PID: %u - Sin hueco contiguo, pidiendo compactacion", pid);

        pthread_mutex_lock(&g_mutex_ks_notif);
        enviar_opcode(g_socket_ks_notificaciones, KM_NOTIF_COMPACTAR);
        pthread_mutex_unlock(&g_mutex_ks_notif);  // fix del autodeadlock

        sem_wait(&sem_compactacion_ok);
        compactar_memoria();

        // Reintentar búsqueda de hueco tras compactación
        pthread_mutex_lock(&g_mutex_huecos);
        idx = seleccionar_hueco(tamanio);
        if (idx == -1) {
            pthread_mutex_unlock(&g_mutex_huecos);
            log_error(logger, "## PID: %u - Sin hueco contiguo tras compactacion", pid);
            enviar_opcode(cliente, RESPUESTA_ERROR);
            return;
        }
        // idx != -1: sigue con el lock tomado, cae al bloque de abajo
    }

    // idx != -1, g_mutex_huecos tomado (ya sea por camino directo o post-compactación)
    uint32_t base = ocupar_hueco(idx, tamanio);
    pthread_mutex_unlock(&g_mutex_huecos);

    char key[20];
    snprintf(key, sizeof(key), "%u", pid);

    pthread_mutex_lock(&g_mutex_procesos);
    t_proceso_memoria* proc = dictionary_get(g_procesos, key);
    if (!proc) {
        pthread_mutex_unlock(&g_mutex_procesos);
        liberar_segmento(base, tamanio);
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }

    t_segmento* seg = malloc(sizeof(t_segmento));
    seg->id_segmento = id_segmento;
    seg->base        = base;
    seg->limite      = tamanio;
    list_add(proc->contexto.tabla_segmentos, seg);
    pthread_mutex_unlock(&g_mutex_procesos);

    log_info(logger, "## PID: %u - Segmento Creado %u - Tamaño: %u",
             pid, id_segmento, tamanio);
    enviar_opcode(cliente, RESPUESTA_OK);
}

void op_mem_free(int cliente)
{
    uint32_t pid, id_segmento;
    recibir_uint32(cliente, &pid);
    recibir_uint32(cliente, &id_segmento);

    char key[20];
    snprintf(key, sizeof(key), "%u", pid);

    pthread_mutex_lock(&g_mutex_procesos);
    t_proceso_memoria* proc = dictionary_get(g_procesos, key);
    if (!proc) {
        pthread_mutex_unlock(&g_mutex_procesos);
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }

    t_segmento* seg_elim = NULL;
    for (int i = 0; i < (int)list_size(proc->contexto.tabla_segmentos); i++) {
        t_segmento* s = list_get(proc->contexto.tabla_segmentos, i);
        if (s->id_segmento == id_segmento) {
            seg_elim = list_remove(proc->contexto.tabla_segmentos, i);
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex_procesos);

    if (!seg_elim) { enviar_opcode(cliente, RESPUESTA_ERROR); return; }

    liberar_segmento(seg_elim->base, seg_elim->limite);
    log_info(logger, "## PID: %u - Segmento %u liberado", pid, id_segmento);
    free(seg_elim);
    enviar_opcode(cliente, RESPUESTA_OK);

    notificar_memoria_libre_al_scheduler();
}

void op_mem_read(int cliente)
{
    // el KS manda: PID + dirección física + tamaño
    // (la CPU ya tradujo lógica→física con su MMU y se lo pasó al KS)
    uint32_t pid, dir_fisica, tamanio;
    recibir_uint32(cliente, &pid);
    recibir_uint32(cliente, &dir_fisica);
    recibir_uint32(cliente, &tamanio);

    log_info(logger, "## PID: %u - Lectura - Dir. Física: %u - Tamaño: %u", pid, dir_fisica, tamanio);

    void* datos = leer_de_sticks(dir_fisica, tamanio);
    if (!datos) { enviar_opcode(cliente, RESPUESTA_ERROR); return; }

    enviar_opcode(cliente, RESPUESTA_OK);
    enviar_buffer(cliente, datos, tamanio);
    free(datos);
}

void op_mem_write(int cliente)
{
    uint32_t pid, dir_fisica, tamanio;
    recibir_uint32(cliente, &pid);
    recibir_uint32(cliente, &dir_fisica);
    recibir_uint32(cliente, &tamanio);

    void* datos = malloc(tamanio);
    recibir_buffer(cliente, datos, tamanio);

    log_info(logger, "## PID: %u - Escritura - Dir. Física: %u - Tamaño: %u", pid, dir_fisica, tamanio);

    int ok = escribir_en_sticks(dir_fisica, datos, tamanio);
    free(datos);
    enviar_opcode(cliente, ok == 0 ? RESPUESTA_OK : RESPUESTA_ERROR);
}

void op_finalizar_proceso(int cliente)
{
    uint32_t pid;
    recibir_uint32(cliente, &pid);

    char key[20];
    snprintf(key, sizeof(key), "%u", pid);

    pthread_mutex_lock(&g_mutex_procesos);
    t_proceso_memoria* proc = dictionary_remove(g_procesos, key);
    pthread_mutex_unlock(&g_mutex_procesos);

    if (!proc) { enviar_opcode(cliente, RESPUESTA_ERROR); return; }

    // liberar todos los segmentos en memoria
    for (int i = 0; i < (int)list_size(proc->contexto.tabla_segmentos); i++) {
        t_segmento* s = list_get(proc->contexto.tabla_segmentos, i);
        liberar_segmento(s->base, s->limite);
    }
    list_destroy_and_destroy_elements(proc->contexto.tabla_segmentos, free);
    list_destroy_and_destroy_elements(proc->instrucciones, free);
    list_destroy_and_destroy_elements(proc->segmentos_suspendidos, destruir_segmento_swap);
    free(proc);

    log_info(logger, "## PID: %u - Proceso finalizado y memoria liberada", pid);
    enviar_opcode(cliente, RESPUESTA_OK);
}

// ── Suspensión / Des-suspensión ───────────────────────────────────

// mueve, de a 1, cada segmento del proceso desde los Memory Sticks hacia
// bloques del modulo SWAP, liberando la memoria principal a medida que
// cada segmento queda copiado.
void op_suspender_proceso(int cliente)
{
    uint32_t pid;
    recibir_uint32(cliente, &pid);

    char key[20];
    snprintf(key, sizeof(key), "%u", pid);

    pthread_mutex_lock(&g_mutex_procesos);
    t_proceso_memoria* proc = dictionary_get(g_procesos, key);
    pthread_mutex_unlock(&g_mutex_procesos);

    if (!proc) { enviar_opcode(cliente, RESPUESTA_ERROR); return; }

    if (g_socket_swap == -1) {
        log_error(logger, "## PID: %u - No es posible suspender: SWAP no conectado", pid);
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }

    t_list* segmentos          = proc->contexto.tabla_segmentos;
    t_list* nuevos_suspendidos = list_create();
    void*   buffer_bloque      = malloc(g_swap_block_size);
    bool    ok                 = true;

    for (int i = 0; i < (int)list_size(segmentos) && ok; i++) {
        t_segmento* seg = list_get(segmentos, i);
        uint32_t cant_bloques = (seg->limite + g_swap_block_size - 1) / g_swap_block_size;

        t_list* bloques = asignar_bloques_swap(cant_bloques);
        if (!bloques) {
            log_error(logger, "## PID: %u - Espacio de SWAP insuficiente", pid);
            ok = false;
            break;
        }

        void* datos = leer_de_sticks(seg->base, seg->limite);
        if (!datos) {
            liberar_bloques_swap(bloques);
            ok = false;
            break;
        }

        for (uint32_t b = 0; b < cant_bloques && ok; b++) {
            uint32_t offset   = b * g_swap_block_size;
            uint32_t restante = seg->limite - offset;
            uint32_t copiar   = (restante < g_swap_block_size) ? restante : g_swap_block_size;

            memset(buffer_bloque, 0, g_swap_block_size);
            memcpy(buffer_bloque, (char*)datos + offset, copiar);

            uint32_t* num_bloque = list_get(bloques, b);
            if (escribir_bloque_swap(*num_bloque, buffer_bloque) != 0) {
                log_error(logger, "## PID: %u - Error escribiendo bloque de SWAP %u", pid, *num_bloque);
                ok = false;
            }
        }
        free(datos);

        if (!ok) { liberar_bloques_swap(bloques); break; }

        liberar_segmento(seg->base, seg->limite);

        t_segmento_swap* ss = malloc(sizeof(t_segmento_swap));
        ss->id_segmento = seg->id_segmento;
        ss->tamanio     = seg->limite;
        ss->bloques     = bloques;
        list_add(nuevos_suspendidos, ss);
    }
    free(buffer_bloque);

    if (!ok) {
        list_destroy_and_destroy_elements(nuevos_suspendidos, destruir_segmento_swap);
        enviar_opcode(cliente, RESPUESTA_ERROR);
        return;
    }

    pthread_mutex_lock(&g_mutex_procesos);
    list_destroy_and_destroy_elements(proc->contexto.tabla_segmentos, free);
    proc->contexto.tabla_segmentos = list_create();
    for (int i = 0; i < (int)list_size(nuevos_suspendidos); i++)
        list_add(proc->segmentos_suspendidos, list_get(nuevos_suspendidos, i));
    list_destroy(nuevos_suspendidos);
    pthread_mutex_unlock(&g_mutex_procesos);

    log_info(logger, "## PID: %u - Proceso suspendido, memoria movida a SWAP", pid);
    enviar_opcode(cliente, RESPUESTA_OK);

    notificar_memoria_libre_al_scheduler();
}

// restaura todos los segmentos suspendidos desde SWAP hacia memoria
// principal, usando el algoritmo de busqueda de huecos configurado,
// y regenera la tabla de segmentos del contexto.
void op_reanudar_proceso(int cliente)
{
    uint32_t pid;
    recibir_uint32(cliente, &pid);

    char key[20];
    snprintf(key, sizeof(key), "%u", pid);

    pthread_mutex_lock(&g_mutex_procesos);
    t_proceso_memoria* proc = dictionary_get(g_procesos, key);
    pthread_mutex_unlock(&g_mutex_procesos);

    if (!proc) { enviar_opcode(cliente, RESPUESTA_ERROR); return; }

    t_list* pendientes = proc->segmentos_suspendidos;

    // verificar que haya espacio para todos los segmentos antes de mover nada
    pthread_mutex_lock(&g_mutex_huecos);
    for (int i = 0; i < (int)list_size(pendientes); i++) {
        t_segmento_swap* ss = list_get(pendientes, i);
        if (seleccionar_hueco(ss->tamanio) == -1) {
            pthread_mutex_unlock(&g_mutex_huecos);
            enviar_opcode(cliente, RESPUESTA_ERROR);
            return;
        }
    }
    pthread_mutex_unlock(&g_mutex_huecos);

    t_list* nuevos_segmentos = list_create();
    void*   buffer_bloque    = malloc(g_swap_block_size);

    for (int i = 0; i < (int)list_size(pendientes); i++) {
        t_segmento_swap* ss = list_get(pendientes, i);

        pthread_mutex_lock(&g_mutex_huecos);
        int      idx       = seleccionar_hueco(ss->tamanio);
        uint32_t nueva_base = ocupar_hueco(idx, ss->tamanio);
        pthread_mutex_unlock(&g_mutex_huecos);

        for (int b = 0; b < (int)list_size(ss->bloques); b++) {
            uint32_t* num_bloque = list_get(ss->bloques, b);
            if (leer_bloque_swap(*num_bloque, buffer_bloque) != 0) {
                log_error(logger, "## PID: %u - Error leyendo bloque de SWAP %u", pid, *num_bloque);
                continue;
            }
            uint32_t offset   = b * g_swap_block_size;
            uint32_t restante = ss->tamanio - offset;
            uint32_t copiar   = (restante < g_swap_block_size) ? restante : g_swap_block_size;

            escribir_en_sticks(nueva_base + offset, buffer_bloque, copiar);
        }

        liberar_bloques_swap(ss->bloques);

        t_segmento* seg = malloc(sizeof(t_segmento));
        seg->id_segmento = ss->id_segmento;
        seg->base        = nueva_base;
        seg->limite      = ss->tamanio;
        list_add(nuevos_segmentos, seg);

        free(ss);
    }
    free(buffer_bloque);

    pthread_mutex_lock(&g_mutex_procesos);
    for (int i = 0; i < (int)list_size(nuevos_segmentos); i++)
        list_add(proc->contexto.tabla_segmentos, list_get(nuevos_segmentos, i));
    list_destroy(nuevos_segmentos);
    list_destroy(proc->segmentos_suspendidos);
    proc->segmentos_suspendidos = list_create();
    pthread_mutex_unlock(&g_mutex_procesos);

    log_info(logger, "## PID: %u - Proceso reanudado, memoria restaurada desde SWAP", pid);
    enviar_opcode(cliente, RESPUESTA_OK);
}

// atender cliente  (hilo por conexión)

void* atender_cliente(void* arg)
{
    t_args_cliente* args = (t_args_cliente*)arg;
    int cliente = args->socket;
    free(args);

    int32_t modulo = handshake_servidor(cliente, logger);
    bool es_notificaciones = false;

    // swap: se conecta una unica vez e informa block_size y tamanio total
    if (modulo == MODULO_SWAP) {
        uint32_t block_size, total_size;
        recibir_uint32(cliente, &block_size);
        recibir_uint32(cliente, &total_size);

        g_swap_block_size    = block_size;
        g_swap_total_bloques = (block_size > 0) ? (total_size / block_size) : 0;

        pthread_mutex_lock(&g_mutex_bloques_swap);
        g_bloques_libres = list_create();
        for (uint32_t i = 0; i < g_swap_total_bloques; i++) {
            uint32_t* b = malloc(sizeof(uint32_t));
            *b = i;
            list_add(g_bloques_libres, b);
        }
        pthread_mutex_unlock(&g_mutex_bloques_swap);

        g_socket_swap = cliente;
        log_info(logger, "## Modulo SWAP Conectado - %u bloques de %u bytes", g_swap_total_bloques, block_size);

        // este hilo solo se usa para detectar la desconexion; las
        // operaciones SW_LEER/SW_ESCRIBIR se disparan sincrónicamente
        // desde los hilos que atienden pedidos de suspension/reanudacion.
        op_code op;
        while (recibir_opcode(cliente, &op) > 0);

        log_error(logger, "## Modulo SWAP desconectado");
        g_socket_swap = -1;
        close(cliente);
        return NULL;
    }

    // memory stick
    if (modulo == MODULO_MEMORY_STICK) {
        uint32_t tamanio;
        recibir_uint32(cliente, &tamanio);

        t_memory_stick* stick = malloc(sizeof(t_memory_stick));
        stick->socket  = cliente;
        stick->tamanio = tamanio;

        pthread_mutex_lock(&g_mutex_sticks);
        pthread_mutex_lock(&g_mutex_huecos);

        // la base del nuevo stick es el total de memoria ya existente
        uint32_t base = 0;
        for (int i = 0; i < (int)list_size(g_memory_sticks); i++)
            base += ((t_memory_stick*)list_get(g_memory_sticks, i))->tamanio;
        stick->base_fisica = base;

        // agregar hueco nuevo al final
        t_hueco* h = malloc(sizeof(t_hueco));
        h->base    = base;
        h->tamanio = tamanio;
        list_add(g_huecos, h);
        fusionar_huecos();

        list_add(g_memory_sticks, stick);

        pthread_mutex_unlock(&g_mutex_huecos);
        pthread_mutex_unlock(&g_mutex_sticks);

        log_info(logger, "## Memory Stick de %u bytes Conectada", tamanio);
        notificar_memoria_libre_al_scheduler();

        // esperar hasta que el stick se desconecte
        op_code op;
        while (recibir_opcode(cliente, &op) > 0);   // loop vacío, el stick no manda nada

        log_error(logger, "## Memory Stick desconectado — notificando BSOD");

        // remover de la lista
        pthread_mutex_lock(&g_mutex_sticks);
        for (int i = 0; i < (int)list_size(g_memory_sticks); i++) {
            if (list_get(g_memory_sticks, i) == stick) {
                list_remove(g_memory_sticks, i);
                break;
            }
        }
        pthread_mutex_unlock(&g_mutex_sticks);
        free(stick);

        notificar_bsod_al_scheduler();
        close(cliente);
        return NULL;
    }

    // kernel scheduler
    // el scheduler se conecta dos veces: 1ª = socket de operaciones,
    // 2ª = socket de notificaciones (el KS se conecta, espera mensajes async)
    if (modulo == MODULO_KERNEL_SCHEDULER) {
        es_notificaciones = (g_socket_ks_operaciones != -1);

        if (!es_notificaciones) {
            g_socket_ks_operaciones = cliente;
            log_info(logger, "## Kernel Scheduler Conectado - FD del socket: %d", cliente);
        } else {
            g_socket_ks_notificaciones = cliente;
            log_info(logger, "## Kernel Scheduler socket notificaciones FD: %d", cliente);
            // este hilo no necesita hacer nada más, el kernel memory escribe en este
            // socket mediante notificar_*. Lo dejamos vivo sin loop.
        }
    }

    // loop principal (cpu y scheduler)
    while (1) {
        op_code operacion;
        if (recibir_opcode(cliente, &operacion) <= 0) {
            log_error(logger, "## Cliente desconectado (fd=%d)", cliente);
            break;
        }

        if (es_notificaciones) {
            log_warning(logger, "Operacion inesperada en socket notificaciones: %d", operacion);
        } else {

        switch (operacion) {
            case KM_CREAR_PROCESO:       op_crear_proceso(cliente);       break;
            case KM_PEDIR_INSTRUCCION:   op_enviar_instruccion(cliente);  break;
            case KM_PEDIR_CONTEXTO:      op_enviar_contexto(cliente);     break;
            case KM_ACTUALIZAR_CONTEXTO: op_actualizar_contexto(cliente); break;
            case KM_MEM_ALLOC:           op_mem_alloc(cliente);           break;
            case KM_MEM_FREE:            op_mem_free(cliente);            break;
            case KM_MEM_READ:            op_mem_read(cliente);            break;
            case KM_MEM_WRITE:           op_mem_write(cliente);           break;
            case KM_FINALIZAR_PROCESO:   op_finalizar_proceso(cliente);   break;
            case KM_SUSPENDER_PROCESO:   op_suspender_proceso(cliente);   break;
            case KM_REANUDAR_PROCESO:    op_reanudar_proceso(cliente);    break;
            case KM_COMPACTACION_OK:     log_info(logger, "## KS confirmo desalojo de CPUs"); sem_post(&sem_compactacion_ok); break;
            default:
                log_error(logger, "## Operacion desconocida: %d", operacion);
                break;
        }
        }
    }

    close(cliente);
    return NULL;
}
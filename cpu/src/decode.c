#include "cpu.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

t_instruccion decode(char* texto) {

    t_instruccion inst = {0};

    char nombre[32] = {0};

    sscanf(texto, "%31s %31s %31s", nombre, inst.param1, inst.param2);

    if (strcasecmp(nombre, "NOOP") == 0) inst.tipo = INST_NOOP;
    else if (strcasecmp(nombre, "SET") == 0) inst.tipo = INST_SET;
    else if (strcasecmp(nombre, "SUM") == 0) inst.tipo = INST_SUM;
    else if (strcasecmp(nombre, "SUB") == 0) inst.tipo = INST_SUB;
    else if (strcasecmp(nombre, "JNZ") == 0) inst.tipo = INST_JNZ;
    else if (strcasecmp(nombre, "MOV_IN") == 0) inst.tipo = INST_MOV_IN;
    else if (strcasecmp(nombre, "MOV_OUT") == 0) inst.tipo = INST_MOV_OUT;
    else if (strcasecmp(nombre, "COPY_MEM") == 0) inst.tipo = INST_COPY_MEM;
    else if (strcasecmp(nombre, "MUTEX_CREATE") == 0) inst.tipo = INST_MUTEX_CREATE;
    else if (strcasecmp(nombre, "MUTEX_LOCK") == 0) inst.tipo = INST_MUTEX_LOCK;
    else if (strcasecmp(nombre, "MUTEX_UNLOCK") == 0) inst.tipo = INST_MUTEX_UNLOCK;
    else if (strcasecmp(nombre, "MEM_ALLOC") == 0) inst.tipo = INST_MEM_ALLOC;
    else if (strcasecmp(nombre, "MEM_FREE") == 0) inst.tipo = INST_MEM_FREE;
    else if (strcasecmp(nombre, "SLEEP") == 0) inst.tipo = INST_SLEEP;
    else if (strcasecmp(nombre, "STDOUT") == 0) inst.tipo = INST_STDOUT;
    else if (strcasecmp(nombre, "STDIN") == 0) inst.tipo = INST_STDIN;
    else if (strcasecmp(nombre, "INIT_PROC") == 0) inst.tipo = INST_INIT_PROC;
    else if (strcasecmp(nombre, "EXIT") == 0) inst.tipo = INST_EXIT;
    else inst.tipo = INST_DESCONOCIDA;

    return inst;
}
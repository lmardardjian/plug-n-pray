#include "cpu.h"
#include <string.h>
#include <utils/mensajes.h>

uint32_t leer_registro(t_contexto* ctx, char* nombre) {
    if (strcmp(nombre, "AX") == 0) return ctx->ax;
    if (strcmp(nombre, "BX") == 0) return ctx->bx;
    if (strcmp(nombre, "CX") == 0) return ctx->cx;
    if (strcmp(nombre, "DX") == 0) return ctx->dx;
    if (strcmp(nombre, "EAX") == 0) return ctx->eax;
    if (strcmp(nombre, "EBX") == 0) return ctx->ebx;
    if (strcmp(nombre, "ECX") == 0) return ctx->ecx;
    if (strcmp(nombre, "EDX") == 0) return ctx->edx;
    if (strcmp(nombre, "SI") == 0)  return ctx->si;
    if (strcmp(nombre, "DI") == 0)  return ctx->di;
    if (strcmp(nombre, "PC") == 0)  return ctx->pc;
    return 0; // registro desconocido
}

void escribir_registro(t_contexto* ctx, char* nombre, uint32_t valor) {
    if (strcmp(nombre, "AX") == 0) { ctx->ax = (uint8_t)valor; return; }
    if (strcmp(nombre, "BX") == 0) { ctx->bx = (uint8_t)valor; return; }
    if (strcmp(nombre, "CX") == 0) { ctx->cx = (uint8_t)valor; return; }
    if (strcmp(nombre, "DX") == 0) { ctx->dx = (uint8_t)valor; return; }
    if (strcmp(nombre, "EAX") == 0) { ctx->eax = valor; return; }
    if (strcmp(nombre, "EBX") == 0) { ctx->ebx = valor; return; }
    if (strcmp(nombre, "ECX") == 0) { ctx->ecx = valor; return; }
    if (strcmp(nombre, "EDX") == 0) { ctx->edx = valor; return; }
    if (strcmp(nombre, "SI") == 0)  { ctx->si = valor; return; }
    if (strcmp(nombre, "DI") == 0)  { ctx->di = valor; return; }
    if (strcmp(nombre, "PC") == 0)  { ctx->pc = valor; return; }
}
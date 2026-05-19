# PROTOCOLO_KM.md
# Módulo Kernel Memory

El módulo Kernel Memory es el encargado de administrar:
* Contextos de ejecución
* Instrucciones de los procesos
* Operaciones de memoria
* Espacio libre disponible (mock en Checkpoint 2)

Durante el Checkpoint 2:
* NO existe memoria real
* NO existen segmentos reales
* NO existen Memory Sticks reales
* Las operaciones MEM_READ y MEM_WRITE responden OK mock
* El espacio libre devuelve un valor fijo mock

-------------------------------------------------------------------------

# Inicio del módulo
El módulo debe iniciarse indicando:

```bash
./bin/kernel_memory [Archivo Config]
```
-------------------------------------------------------------------------

# Funcionamiento general
1. Kernel Memory inicia un servidor multihilo.
2. Espera conexiones concurrentes.
3. Cada cliente conectado se atiende en un hilo independiente.
4. Los módulos clientes pueden ser:
   * Kernel Scheduler
   * CPU
5. El cliente envía:
   * Opcode
   * Payload correspondiente
6. Kernel Memory ejecuta la operación.
7. Kernel Memory responde según corresponda.

-------------------------------------------------------------------------

# Operaciones soportadas (Checkpoint 2)

| Operación | Descripción |
|--------------------       |--------------------|
| `KM_CREAR_PROCESO`        | Crea estructura del proceso y carga instrucciones |
| `KM_PEDIR_INSTRUCCION`    | Devuelve instrucción según PID y PC |
| `KM_PEDIR_CONTEXTO`       | Devuelve contexto del proceso |
| `KM_ACTUALIZAR_CONTEXTO`  | Actualiza contexto del proceso |
| `KM_MEM_READ`             | Responde OK mock |
| `KM_MEM_WRITE`            | Responde OK mock |
| `KM_ESPACIO_LIBRE`        | Devuelve espacio libre mock |

-------------------------------------------------------------------------
# KM_CREAR_PROCESO
Recibe:
```c
PID
PATH
```

Hace:
* crea struct proceso
* inicializa registros en 0
* lee instrucciones del archivo
* guarda todo en dictionary

Responde:
```c
RESPUESTA_OK
```

-------------------------------------------------------------------------
# KM_PEDIR_INSTRUCCION
Recibe:
```c
PID
PC
```

Hace:
* busca proceso
* busca instrucción en lista
* la envía

Responde:
```c
char* instruccion
```

-------------------------------------------------------------------------
# KM_PEDIR_CONTEXTO
Recibe:
```c
PID
```

Hace:
* busca el contexto
* lo serializa y lo manda

Responde:
```c
t_contexto
```

-------------------------------------------------------------------------
# KM_ACTUALIZAR_CONTEXTO
Recibe:
```c
PID
t_contexto
```

Hace:
* recibe contexto nuevo
* pisa el contexto viejo

Responde:
```c
RESPUESTA_OK
```

-------------------------------------------------------------------------
# KM_MEM_READ
Checkpoint 2:
* mock
* responde OK

-------------------------------------------------------------------------
# KM_MEM_WRITE
Checkpoint 2:
* mock
* responde OK

-------------------------------------------------------------------------
# KM_ESPACIO_LIBRE
Checkpoint 2:
* devuelve un número fijo mock

-------------------------------------------------------------------------
# Logs importantes
```txt
## PID: X - Proceso Creado

## PID: X - Obtener instruccion: PC - Instruccion: ...

Contexto enviado PID X

Contexto actualizado PID X

MEM_READ mock

MEM_WRITE mock
```
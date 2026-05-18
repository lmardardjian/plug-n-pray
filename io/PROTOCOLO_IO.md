# PROTOCOLO_IO.md
# Módulo IO

El módulo IO es el encargado de simular operaciones de Entrada/Salida del sistema.
Actualmente existen 3 tipos de interfaz soportadas:
* `SLEEP`
* `STDIN`
* `STDOUT`

Cada instancia de IO se conecta al Kernel Scheduler y permanece bloqueada esperando solicitudes de ejecución.

-------------------------------------------------------------------------
# Inicio del módulo

El módulo debe iniciarse indicando:
```bash
./bin/io [Archivo Config] [Tipo]
```

Ejemplos:
./bin/io io.config SLEEP
./bin/io io.config STDOUT
./bin/io io.config STDIN

-------------------------------------------------------------------------
# Funcionamiento general

1. IO se conecta al Kernel Scheduler.
2. Realiza handshake enviando `MODULO_IO`.
3. Entra en loop infinito esperando operaciones.
4. El Kernel envía:
   * Opcode
   * PID
   * Payload/Argumentos
    en ese orden
5. IO ejecuta la operación correspondiente.
6. IO responde:
   * `IO_FINALIZADA` en caso de exito
   * o `IO_ERROR` en caso de error

-------------------------------------------------------------------------
# Orden esperado de recepción

El módulo IO espera recibir los datos en el siguiente orden:

## 1. Opcode
```c
IO_EJECUTAR
```

## 2. PID
```c
int pid
```

## 3. Payload
```c
char* argumentos
```
El contenido del payload depende del tipo de interfaz.

-------------------------------------------------------------------------

# Payload esperado por tipo
## SLEEP
Recibe tiempo en milisegundos.
Ejemplo:
```txt
3000
```

-------------------------------------------------------------------------

## STDOUT
Recibe el texto a imprimir.
Ejemplo:
```txt
Hola Mundo
```

-------------------------------------------------------------------------

## STDIN
Recibe la cantidad de bytes a leer. Si el texto input es mayor a 20 caracteres
lo recorta para que queden 20. Si el texto input es menor a 20 rellena con barra ceros
Ejemplo:
```txt
20
```

-------------------------------------------------------------------------

# Ejemplo de uso desde Kernel
## Ejemplo SLEEP
```c
enviar_opcode(socket_io, IO_EJECUTAR);

enviar_int(socket_io, pid);

enviar_mensaje(socket_io, "3000", logger);
```

-------------------------------------------------------------------------

## Ejemplo STDOUT
```c
enviar_opcode(socket_io, IO_EJECUTAR);

enviar_int(socket_io, pid);

enviar_mensaje(socket_io, "Hola Mundo", logger);
```

-------------------------------------------------------------------------

## Ejemplo STDIN
```c
enviar_opcode(socket_io, IO_EJECUTAR);

enviar_int(socket_io, pid);

enviar_mensaje(socket_io, "20", logger);
```

-------------------------------------------------------------------------

# Respuestas del módulo IO
## Operación exitosa
```c
IO_FINALIZADA
```

-------------------------------------------------------------------------

## Error
```c
IO_ERROR
```

-------------------------------------------------------------------------

# Logs obligatorios implementados
## Conexión a Kernel Scheduler
```txt
## Conectado a Kernel Scheduler
```

## Inicio de IO
```txt
## PID: <PID> - Inicio de IO
```

## Fin de IO
```txt
## PID: <PID> - Fin de IO
```

## STDOUT
```txt
## PID: <PID> - <CONTENIDO>
```

## STDIN
```txt
## PID: <PID> - Ingrese <CANTIDAD> caracteres:
```

## SLEEP
```txt
## PID: <PID> - Haciendo sleep por <TIEMPO> milisegundos.
```

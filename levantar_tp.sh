#!/bin/bash
# ============================================================================
#  levantar_tp.sh — Levanta los módulos del TP "Bobby Tables" en orden, cada
#  uno en su propia terminal, esperando a que sus dependencias (locales o de
#  la otra máquina) estén disponibles antes de arrancar.
#
#  IMPORTANTE: las IPs, puertos, tamaños, algoritmo de planificación, método
#  de asignación de bloques, etc. NO se tocan acá — viven en los .config de
#  cada módulo, que son la única fuente de verdad. Este script los LEE de ahí.
#  Lo único que configurás acá son cosas que no existen en ningún .config
#  porque son argumentos de línea de comandos del binario (proceso inicial,
#  tamaño del memory stick, id de CPU, tipo de IO).
#
#  USO:
#    ./levantar_tp.sh                                  -> pregunta qué levantar
#    ./levantar_tp.sh kernel_memory memory_stick swap   -> levanta esos módulos
#    ./levantar_tp.sh --proceso /ruta/proceso.txt kernel_scheduler
#    ./levantar_tp.sh --cpu-id CPU2 --io-tipo STDIN cpu io
#    ./levantar_tp.sh --cfg cpu=cpu/src/cpu2.config cpu   -> usar otro config
#
#  Cada máquina corre SU PROPIA COPIA de este script y de los .config de los
#  módulos que le toca levantar a ella (con las IPs correspondientes adentro
#  del .config, no acá).
# ============================================================================

set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ============================ CONFIGURACIÓN =================================
# Cosas que NO están en ningún archivo .config (son argv del binario, no
# claves de config). Podés editarlas acá o pasarlas como flag al correr el
# script (los flags pisan estos valores por defecto).

PROCESO_INICIAL="/home/utnso/scripts/proceso_inicial.txt"   # arg de kernel_scheduler
TAMANIO_MEMORY_STICK="4096"                                  # arg de memory_stick (bytes)
ID_CPU="CPU1"                                                 # arg de cpu (nombre del log)
TIPO_IO="STDOUT"                                              # arg de io (SLEEP | STDOUT | STDIN)

# Ruta de cada .config. Estos SÍ son la fuente de verdad de IP/puerto/tamaño/
# algoritmo/etc — el script los lee, no los reescribe. Cambiá estas rutas si
# tenés varios configs (por ej. uno por CPU) o pasá --cfg modulo=ruta.
declare -A CFG=(
  [kernel_memory]="$ROOT_DIR/kernel_memory/src/kernel_memory.config"
  [memory_stick]="$ROOT_DIR/memory_stick/src/memory_stick.config"
  [swap]="$ROOT_DIR/swap/src/swap.config"
  [kernel_scheduler]="$ROOT_DIR/kernel_scheduler/src/kernel_scheduler.config"
  [cpu]="$ROOT_DIR/cpu/src/cpu.config"
  [io]="$ROOT_DIR/io/src/io.config"
)

# Cuánto espera (en segundos) cada dependencia de red antes de rendirse y
# arrancar igual (para no colgarse eterno si algo quedó mal levantado).
TIMEOUT_ESPERA=120

# Segundos entre lanzar un módulo y el siguiente.
DELAY_ENTRE_MODULOS=1

# ==================== NO HACE FALTA TOCAR DE ACÁ PARA ABAJO =================

# Orden fijo de arranque, respeta las dependencias reales del protocolo:
# KM primero (todos se conectan a él) -> Memory Stick y Swap (clientes de KM)
# -> Kernel Scheduler (cliente de KM, crea el proceso inicial al arrancar)
# -> CPU (cliente de KS y KM) -> IO (cliente de KS).
ORDEN=(kernel_memory memory_stick swap kernel_scheduler cpu io)

declare -A BIN=(
  [kernel_memory]="$ROOT_DIR/kernel_memory/bin/kernel_memory"
  [memory_stick]="$ROOT_DIR/memory_stick/bin/memory_stick"
  [swap]="$ROOT_DIR/swap/bin/swap"
  [kernel_scheduler]="$ROOT_DIR/kernel_scheduler/bin/kernel_scheduler"
  [cpu]="$ROOT_DIR/cpu/bin/cpu"
  [io]="$ROOT_DIR/io/bin/io"
)

# Qué claves de IP:PUERTO hay que leer del PROPIO .config de cada módulo para
# saber a quién debe esperar antes de arrancar (un módulo puede depender de
# más de uno, separados por espacio). Vacío = no espera nada (es servidor
# "raíz", como kernel_memory).
declare -A DEP_KEYS=(
  [kernel_memory]=""
  [memory_stick]="IP_KERNEL_MEMORY:PUERTO_KERNEL_MEMORY"
  [swap]="IP_KERNEL_MEMORY:PUERTO_KERNEL_MEMORY"
  [kernel_scheduler]="IP_KERNEL_MEMORY:PUERTO_KERNEL_MEMORY"
  [cpu]="IP_KERNEL_SCHEDULER:PUERTO_KERNEL_SCHEDULER IP_KERNEL_MEMORY:PUERTO_KERNEL_MEMORY"
  [io]="IP_KERNEL_SCHEDULER:PUERTO_KERNEL_SCHEDULER"
)

# ------------------------------- Utilidades ---------------------------------

es_modulo_valido() {
  local candidato="$1"
  for m in "${ORDEN[@]}"; do
    [ "$m" == "$candidato" ] && return 0
  done
  return 1
}

# Lee el valor de CLAVE=valor de un archivo .config (formato commons: sin
# comillas, una clave por línea). Devuelve vacío si no la encuentra.
leer_config() {
  local archivo="$1"
  local clave="$2"
  [ -f "$archivo" ] || return 1
  grep -E "^${clave}[[:space:]]*=" "$archivo" | tail -n1 | cut -d'=' -f2- | tr -d '[:space:]"'
}

# Arma la lista de "host:puerto" de los que depende un módulo, leyendo las
# claves correspondientes del .config de ESE módulo.
resolver_dependencias() {
  local mod="$1"
  local cfg="${CFG[$mod]}"
  local pares="${DEP_KEYS[$mod]:-}"
  local resultado=()

  for par in $pares; do
    local ip_key="${par%%:*}"
    local port_key="${par##*:}"
    local ip val_port
    ip=$(leer_config "$cfg" "$ip_key")
    val_port=$(leer_config "$cfg" "$port_key")
    if [ -n "$ip" ] && [ -n "$val_port" ]; then
      resultado+=("$ip:$val_port")
    else
      echo "   !! No pude leer $ip_key/$port_key desde $cfg (¿existe y tiene esas claves?)" >&2
    fi
  done

  echo "${resultado[@]}"
}

# Dice si un host es "esta misma máquina" (localhost o alguna IP propia).
es_localhost() {
  local host="$1"
  case "$host" in
    127.0.0.1|localhost|0.0.0.0) return 0 ;;
  esac
  local propias
  propias=$(hostname -I 2>/dev/null)
  for ip in $propias; do
    [ "$ip" == "$host" ] && return 0
  done
  return 1
}

# Chequea si hay algo en LISTEN en ese puerto EN ESTA MÁQUINA, sin abrir
# ninguna conexión (así el módulo servidor no ve ningún cliente fantasma
# y no logea ningún "handshake fallido"). Devuelve 2 si no puede determinarlo
# (ni ss ni /proc/net/tcp disponibles), para que el llamador haga fallback.
puerto_escuchando_local() {
  local port="$1"
  if command -v ss >/dev/null 2>&1; then
    ss -ltn 2>/dev/null | awk '{print $4}' | grep -qE "[:.]${port}\$"
    return $?
  elif [ -r /proc/net/tcp ]; then
    local hexport
    hexport=$(printf '%04X' "$port")
    awk -v p=":${hexport}\$" '$2 ~ p && $4=="0A" {found=1} END{exit !found}' /proc/net/tcp
    return $?
  fi
  return 2
}

# Espera a que un módulo esté disponible. Si es local, mira el estado del
# puerto directamente (sin tocar la red, cero ruido en el log del módulo
# esperado). Si es remoto, no queda otra que probar con una conexión TCP real
# -- el módulo del otro lado va a ver esa conexión y loguear un intento de
# handshake fallido; es inofensivo (no ocupa ningún socket persistente, solo
# ensucia un poco el log), simplemente no hay forma de chequear un puerto
# ajeno sin tocarlo.
esperar_puerto() {
  local hostport="$1"
  local host="${hostport%%:*}"
  local port="${hostport##*:}"
  local i=0

  if es_localhost "$host"; then
    echo "   -> Esperando a que el puerto $port esté escuchando en esta máquina (chequeo local, sin conectarme)..."
    while true; do
      puerto_escuchando_local "$port"
      local estado=$?
      if [ "$estado" -eq 0 ]; then
        echo "   -> Puerto $port está escuchando. Sigo."
        return 0
      elif [ "$estado" -eq 2 ]; then
        echo "   -> No tengo 'ss' ni /proc/net/tcp para chequear localmente, uso conexión TCP como respaldo."
        break
      fi
      i=$((i + 1))
      if [ "$i" -ge "$TIMEOUT_ESPERA" ]; then
        echo "   !! TIMEOUT esperando el puerto $port local (${TIMEOUT_ESPERA}s). Arranco igual, revisá que ese módulo esté levantado."
        return 1
      fi
      sleep 1
    done
    i=0
  else
    echo "   -> Esperando a que $host:$port (otra máquina) responda..."
  fi

  while true; do
    if (exec 3<>"/dev/tcp/$host/$port") 2>/dev/null; then
      exec 3<&- 2>/dev/null
      exec 3>&- 2>/dev/null
      echo "   -> $host:$port responde. Sigo."
      return 0
    fi
    i=$((i + 1))
    if [ "$i" -ge "$TIMEOUT_ESPERA" ]; then
      echo "   !! TIMEOUT esperando $host:$port (${TIMEOUT_ESPERA}s). Arranco igual, revisá que ese módulo esté levantado."
      return 1
    fi
    sleep 1
  done
}

# Abre una terminal gráfica nueva ejecutando $2 (busca el emulador disponible;
# si no encuentra ninguno, corre en background con el log en logs/<titulo>.log)
abrir_terminal() {
  local titulo="$1"
  local comando="$2"
  local wrapper="$comando; ec=\$?; echo; echo \"[$titulo] terminó (código \$ec). Presioná ENTER para cerrar esta terminal.\"; read _"

  if command -v gnome-terminal >/dev/null 2>&1; then
    gnome-terminal --title="$titulo" -- bash -c "$wrapper"
  elif command -v konsole >/dev/null 2>&1; then
    konsole -p tabtitle="$titulo" -e bash -c "$wrapper" &
  elif command -v xfce4-terminal >/dev/null 2>&1; then
    xfce4-terminal --title="$titulo" -e "bash -c '$wrapper'" &
  elif command -v xterm >/dev/null 2>&1; then
    xterm -T "$titulo" -e bash -c "$wrapper" &
  elif command -v x-terminal-emulator >/dev/null 2>&1; then
    x-terminal-emulator -T "$titulo" -e bash -c "$wrapper" &
  else
    mkdir -p "$ROOT_DIR/logs"
    echo "   (no hay emulador de terminal gráfico disponible; corro en background, log en logs/$titulo.log)"
    nohup bash -c "$comando" > "$ROOT_DIR/logs/$titulo.log" 2>&1 &
  fi
}

# Arma los argv extra de cada módulo (los que no están en ningún .config).
argumentos_de() {
  local mod="$1"
  case "$mod" in
    memory_stick) echo "$TAMANIO_MEMORY_STICK" ;;
    kernel_scheduler) echo "$PROCESO_INICIAL" ;;
    cpu) echo "$ID_CPU" ;;
    io) echo "$TIPO_IO" ;;
    *) echo "" ;;
  esac
}

lanzar_modulo() {
  local mod="$1"
  local bin="${BIN[$mod]}"
  local cfg="${CFG[$mod]}"
  local args
  args=$(argumentos_de "$mod")

  echo "==> $mod"

  if [ ! -x "$bin" ]; then
    echo "   !! No encuentro el binario compilado: $bin"
    echo "   !! Compilalo con 'make' dentro de $(dirname "$(dirname "$bin")") y volvé a correr el script."
    return 1
  fi
  if [ ! -f "$cfg" ]; then
    echo "   !! No encuentro el config: $cfg"
    return 1
  fi

  local deps
  deps=$(resolver_dependencias "$mod")
  if [ -n "$deps" ]; then
    for hp in $deps; do
      esperar_puerto "$hp"
    done
  fi

  local comando
  comando="cd '$(dirname "$(dirname "$bin")")' && '$bin' '$cfg' $args"

  echo "   -> Lanzando: $(basename "$bin") $(basename "$cfg") $args"
  abrir_terminal "$mod" "$comando"
  sleep "$DELAY_ENTRE_MODULOS"
}

mostrar_ayuda() {
  cat <<EOF
Uso: $0 [flags] [modulos...]

Flags:
  --proceso RUTA       Path del proceso inicial (arg de kernel_scheduler)
  --stick-size N        Tamaño en bytes del memory stick (arg de memory_stick)
  --cpu-id ID            Identificador de la CPU (arg de cpu)
  --io-tipo TIPO          Tipo de interfaz IO: SLEEP | STDOUT | STDIN
  --cfg modulo=RUTA       Usar otro .config para ese módulo (repetible)
  -h, --help              Esta ayuda

Módulos válidos: ${ORDEN[*]}

Nota sobre las esperas: si el módulo del que se depende corre en ESTA
máquina, se chequea con 'ss'/proc (sin abrir conexión), así el servidor
no ve ningún cliente fantasma en su log. Si corre en otra máquina, no
queda otra que probar con una conexión TCP real -- vas a ver un intento
de handshake fallido en el log del módulo remoto, es inofensivo.

Ejemplos:
  $0 kernel_memory memory_stick swap
  $0 --proceso /home/utnso/scripts/proceso1.txt kernel_scheduler
  $0 --cpu-id CPU2 --cfg cpu=cpu/src/cpu2.config cpu
EOF
}

# --------------------------------- Main --------------------------------------

main() {
  local seleccion=()

  while [ "$#" -gt 0 ]; do
    case "$1" in
      --proceso)
        PROCESO_INICIAL="$2"; shift 2 ;;
      --stick-size)
        TAMANIO_MEMORY_STICK="$2"; shift 2 ;;
      --cpu-id)
        ID_CPU="$2"; shift 2 ;;
      --io-tipo)
        TIPO_IO="$2"; shift 2 ;;
      --cfg)
        local modulo="${2%%=*}"
        local ruta="${2#*=}"
        if ! es_modulo_valido "$modulo"; then
          echo "!! '--cfg $2' inválido: '$modulo' no es un módulo conocido."
          exit 1
        fi
        CFG["$modulo"]="$ruta"
        shift 2 ;;
      -h|--help)
        mostrar_ayuda; exit 0 ;;
      *)
        seleccion+=("$1"); shift ;;
    esac
  done

  if [ "${#seleccion[@]}" -eq 0 ]; then
    echo "Módulos disponibles: ${ORDEN[*]}"
    echo "¿Cuáles querés levantar EN ESTA MÁQUINA? (separados por espacio, ENTER vacío = cancelar)"
    read -rp "> " -a seleccion
  fi

  if [ "${#seleccion[@]}" -eq 0 ]; then
    echo "No se seleccionó ningún módulo. Saliendo."
    exit 0
  fi

  for s in "${seleccion[@]}"; do
    if ! es_modulo_valido "$s"; then
      echo "!! '$s' no es un módulo válido. Opciones: ${ORDEN[*]}"
      exit 1
    fi
  done

  echo
  echo "Se van a levantar, en este orden:"
  for mod in "${ORDEN[@]}"; do
    for s in "${seleccion[@]}"; do
      [ "$mod" == "$s" ] && echo "  - $mod  (config: ${CFG[$mod]})"
    done
  done
  echo

  for mod in "${ORDEN[@]}"; do
    for s in "${seleccion[@]}"; do
      if [ "$mod" == "$s" ]; then
        lanzar_modulo "$mod"
      fi
    done
  done

  echo "Listo. Revisá cada terminal para confirmar que cada módulo conectó bien."
}

main "$@"
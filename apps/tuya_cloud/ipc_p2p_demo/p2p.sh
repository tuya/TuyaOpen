#!/usr/bin/env bash
#
# TuyaOpen T5AI P2P demo helper: build / flash / monitor without retyping the
# tos.py incantation.
#
# Usage:
#   ./p2p.sh                 # build + flash + monitor
#   ./p2p.sh build           # build only
#   ./p2p.sh flash           # flash only
#   ./p2p.sh monitor         # interactive serial monitor (needs a TTY)
#   ./p2p.sh capture         # non-interactive serial capture (background-safe)
#   ./p2p.sh clean           # clean the build directory
#
# Options:
#   -p, --port DEVICE        Serial port (default: flash uses the T5AI
#                            bootloader UART if00, monitor/capture use the
#                            console if02, else the first /dev/ttyACM* or
#                            /dev/ttyUSB*)
#   -b, --baud BAUD          Serial baud rate (default: 460800)
#   -l, --log FILE           Save the serial log to FILE (monitor/capture)
#   -t, --secs SECONDS       capture duration (default: 180)
#       --tos PATH           tos.py path (default: repo root next to apps/)
#   -h, --help               Show this help
#
# Environment:
#   P2P_PORT, P2P_BAUD, P2P_LOG, P2P_CAPTURE_SECS, TOS
#
# Examples:
#   ./p2p.sh
#   ./p2p.sh flash -p /dev/ttyACM1
#   ./p2p.sh capture -l /tmp/t5ai.log -t 300
#   P2P_PORT=/dev/ttyACM0 ./p2p.sh monitor
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

ACTION="all"
PORT="${P2P_PORT:-}"
BAUD="${P2P_BAUD:-460800}"
LOG="${P2P_LOG:-}"
CAPTURE_SECS="${P2P_CAPTURE_SECS:-180}"
TOS="${TOS:-$(cd "$ROOT/../../.." && pwd)/tos.py}"

usage() {
    cat <<'EOF'
Usage: ./p2p.sh [action] [options]

Actions:
  all          build + flash + monitor (default)
  build        build only
  flash        flash only
  monitor      interactive serial monitor (needs a TTY)
  capture      non-interactive serial capture, prints and optionally logs
  clean        clean the build directory

Options:
  -p, --port DEVICE   serial port (default: if00 for flash, if02 for console)
  -b, --baud BAUD     serial baud rate (default: 460800)
  -l, --log FILE      save the serial log to FILE
  -t, --secs SECONDS  capture duration (default: 180)
      --tos PATH      tos.py path (default: the repo root)
  -h, --help          show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port)  PORT="${2:?--port needs a device}"; shift 2 ;;
        -b|--baud)  BAUD="${2:?--baud needs a value}"; shift 2 ;;
        -l|--log)   LOG="${2:?--log needs a path}"; shift 2 ;;
        -t|--secs)  CAPTURE_SECS="${2:?--secs needs a value}"; shift 2 ;;
        --tos)      TOS="${2:?--tos needs a path}"; shift 2 ;;
        -h|--help)  usage; exit 0 ;;
        all|build|flash|monitor|capture|clean)
                    ACTION="$1"; shift ;;
        *)          echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

detect_port() {
    local kind="$1" p
    # T5AI boards with the dual CH340 expose two ports: if00 is the
    # bootloader/flash UART, if02 is the application console.
    if [[ "$kind" == "flash" ]]; then
        for p in /dev/serial/by-id/*-if00; do
            [[ -e "$p" ]] && { readlink -f "$p"; return 0; }
        done
    else
        for p in /dev/serial/by-id/*-if02; do
            [[ -e "$p" ]] && { readlink -f "$p"; return 0; }
        done
    fi
    for p in /dev/ttyACM* /dev/ttyUSB*; do
        [[ -e "$p" ]] && { printf '%s\n' "$p"; return 0; }
    done
    return 1
}

# A user-supplied -p wins for every action; otherwise pick the right UART.
flash_port()   { if [[ -n "$PORT" ]]; then printf '%s\n' "$PORT"; else detect_port flash; fi; }
console_port() { if [[ -n "$PORT" ]]; then printf '%s\n' "$PORT"; else detect_port console; fi; }

if [[ ! -f "$TOS" ]]; then
    echo "error: tos.py not found: $TOS" >&2
    exit 1
fi

FLASH_PORT=""
CONSOLE_PORT=""
case "$ACTION" in
    all)
        FLASH_PORT="$(flash_port || true)"
        CONSOLE_PORT="$(console_port || true)"
        if [[ -z "$FLASH_PORT" || -z "$CONSOLE_PORT" ]]; then
            echo "error: no serial port found; pass -p /dev/ttyACM0" >&2
            exit 1
        fi
        ;;
    flash)
        FLASH_PORT="$(flash_port || true)"
        if [[ -z "$FLASH_PORT" ]]; then
            echo "error: no flash port found; pass -p /dev/ttyACM0" >&2
            exit 1
        fi
        ;;
    monitor|capture)
        CONSOLE_PORT="$(console_port || true)"
        if [[ -z "$CONSOLE_PORT" ]]; then
            echo "error: no console port found; pass -p /dev/ttyACM1" >&2
            exit 1
        fi
        ;;
esac

echo ">> action=$ACTION flash=${FLASH_PORT:-n/a} console=${CONSOLE_PORT:-n/a} baud=$BAUD tos=$TOS"

monitor_args=(-p "$CONSOLE_PORT" -b "$BAUD")
if [[ -n "$LOG" ]]; then
    monitor_args+=(-l "$LOG")
fi

capture() {
    local py=python3
    if ! "$py" -c 'import serial' >/dev/null 2>&1; then
        py="$(cd "$ROOT/../../.." && pwd)/.venv/bin/python"
    fi
    # exec so that stopping the helper (Ctrl-C, TaskStop) kills the reader
    # itself; otherwise a stray python keeps the tty open and silently splits
    # the console stream with the next capture.
    exec "$py" - "$CONSOLE_PORT" "$BAUD" "$CAPTURE_SECS" "$LOG" <<'PY'
import sys
import time

import serial

port, baud, secs, log = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), sys.argv[4]
out = open(log, "ab") if log else None
end = time.time() + secs


def emit(data):
    if not data:
        return
    if out:
        out.write(data)
        out.flush()
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()


try:
    while time.time() < end:
        ser = None
        try:
            ser = serial.Serial(port, baud, timeout=0.5)
            try:
                ser.setDTR(False)
                ser.setRTS(False)
            except Exception:
                pass
            while time.time() < end:
                emit(ser.read(4096))
        except serial.SerialException:
            # The CH340 occasionally drops the interface; reopen and keep
            # capturing instead of losing the whole run.
            time.sleep(0.5)
        finally:
            if ser is not None:
                try:
                    ser.close()
                except Exception:
                    pass
finally:
    if out:
        out.close()
PY
}

case "$ACTION" in
    build)
        exec "$TOS" build
        ;;
    flash)
        exec "$TOS" flash -p "$FLASH_PORT" -b "$BAUD"
        ;;
    monitor)
        exec "$TOS" monitor "${monitor_args[@]}"
        ;;
    capture)
        capture
        ;;
    clean)
        exec "$TOS" clean
        ;;
    all)
        "$TOS" build
        "$TOS" flash -p "$FLASH_PORT" -b "$BAUD"
        sleep 1
        exec "$TOS" monitor "${monitor_args[@]}"
        ;;
esac

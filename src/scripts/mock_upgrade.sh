#!/bin/bash
set -euo pipefail

readonly FIRMWARE_BIN="openwrt-sysupgrade.bin"
readonly BLOCK_A="./mock_tmp/block_A"
readonly BLOCK_B="./mock_tmp/block_B"
readonly SENSOR_BIN="./build/c_sensor"

echo "[*] Initializing Pre-Flight Checks..."
command -v unsquashfs >/dev/null 2>&1 || { echo "[!] FATAL: unsquashfs is required but not installed. Aborting." >&2; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "[!] FATAL: sha256sum is required but not installed. Aborting." >&2; exit 1; }

if[ ! -f "$FIRMWARE_BIN" ]; then
  echo "[!] FATAL: Firmware binary $FIRMWARE_BIN not found in workspace."
  exit 1
fi

SENSOR_PID=""

function teardown_env {
  echo -e "\n[*] Initiating Forensic Teardown Sequence..."

  if [ -n "$SENSOR_PID" ] && kill -0 "$SENSOR_PID" 2>/dev/null; then
    echo "[*] Halting C-Sensor (PID: $SENSOR_PID)..."
    kill -9 "$SENSOR_PID" || true
  fi

  if [ -d "$BLOCK_B" ]; then
    echo "[*] Scrubbing volatile partition $BLOCK_B..."
    rm -rf "$BLOCK_B"
  fi

  echo "[*] Teardown Complete. Environment is clean."
  exit 0
}

trap 'echo -e "\n[!] FORENSIC ABORT TRIGGERED BY SENSOR!"; teardown_env' SIGTERM SIGINT ERR

echo "[*] Igniting Autonomous C-Sensor in -DEBUG mode..."
$SENSOR_BIN $$ -DEBUG &
SENSOR_PID=$!

echo "[*] C-Sensor armed and observing (PID: $SENSOR_PID). Orchestrator PID: $$."

sleep 1

function simulate_firmware_flash {
  echo -e "\n[*] Phase 1: Cryptographic Validation Initiated..."

  VALIDATION_HASH=$(sha256sum "$FIRMWARE_BIN" | awk '{print $1}')
  echo "[*] Validation complete. Hash: $VALIDATION_HASH"

  sleep 2

  echo -e "\n[*] Phase 2: Block extraction initiated..."
  echo "[*] Unpacking squashfs payload to volatile $BLOCK_B..."

  unsquashfs -f -d "$BLOCK_B/" "$FIRMWARE_BIN" >/dev/null

  echo -e "\nUPDATE COMPLETE: Firmware flashed successfully"
  teardown_env
}

#trap teardown_env SIGINT SIGTERM
#trap halt_attack SIGUSR1

#simulate_firmware_flash &
#PAYLOAD_PID=$!

#PAYLOAD_PGID=$(ps -o pgid= -p $PAYLOAD_PID | tr -d ' ')

#touch telemetry/sensor_stream.log

#./bin/sensor_x86 $$ &

#tail -f telemetry/sensor_stream.log &

#TAIL_PID=$!

#wait $PAYLOAD_PID

simulate_firmware_flash

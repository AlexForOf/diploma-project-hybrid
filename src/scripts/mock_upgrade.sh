#!/bin/bash
set -e

set -m

function teardown_env {
  kill $TAIL_PID 2>/dev/null || true
  sudo umount ./mock_tmp || echo "Already unmounted"
  exit 0
}

function halt_attack {
  REASON=$(tail -n 1 telemetry/sensor_stream.log)
  echo -e "\nATTACK HALTED: $REASON"

  kill -9 -$PAYLOAD_PGID 2>/dev/null
  teardown_env
}

function simulate_firmware_flash {
  echo -e "\n[SYSUPGRADE] Staging payload in RAM..."
  dd if=/dev/urandom of=./mock_tmp/dummy_firmware.bin bs=1M count=20 status=none

  echo -e "\n[SYSUPGRADE] Validating signature (5-seconds TOCTOU window)..."
  sleep 5

  echo -e "\n[SYSUPGRADE] Writing to non-volatile flash..."
  cp ./mock_tmp/dummy_firmware.bin ./mock_flash/

  echo -e "\nUPDATE COMPLETE: Firmware flashed successfully"
  teardown_env
}

trap teardown_env SIGINT SIGTERM
trap halt_attack SIGUSR1

simulate_firmware_flash &
PAYLOAD_PID=$!

PAYLOAD_PGID=$(ps -o pgid= -p $PAYLOAD_PID | tr -d ' ')

touch telemetry/sensor_stream.log

./bin/sensor_x86 $$ &

tail -f telemetry/sensor_stream.log &

TAIL_PID=$!

wait $PAYLOAD_PID


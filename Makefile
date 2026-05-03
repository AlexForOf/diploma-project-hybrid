# Thesis Lab Environment Director

# Architecture Variables
CC = gcc
CFLAGS = -Wall -Wextra -O2
SRC_C = src/c
SRC_BASH = src/scripts
BIN_DIR = bin
TELEMETRY_DIR = telemetry

# Mock Environment Variables
TMP_DIR = mock_tmp
FLASH_DIR = mock_flash
UID = $(shell id -u)
GID = $(shell id -g)

.PHONY: all build lab-up lab-down run-mock clean

all: build lab-up

# Compiles the C telemetry sensor
build:
	@echo "🔨 Compiling C Sensor..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC_C)/sensor.c -o $(BIN_DIR)/sensor_x86
	@echo "✅ Build complete."

# Mounts the volatile RAM disk to simulate OpenWrt's /tmp
lab-up:
	@echo "🚀 Provisioning Lab Environment..."
	@mkdir -p $(TMP_DIR) $(FLASH_DIR) $(TELEMETRY_DIR)
	@if mount | grep $(TMP_DIR) > /dev/null; then \
		echo "✅ tmpfs already mounted."; \
	else \
		echo "🔧 Mounting tmpfs to $(TMP_DIR)..."; \
		sudo mount -t tmpfs -o size=100M,uid=$(UID),gid=$(GID) tmpfs $(TMP_DIR); \
		echo "✅ tmpfs mounted successfully."; \
	fi

# Safely unmounts the RAM disk
lab-down:
	@echo "🛑 Tearing down Lab Environment..."
	@if mount | grep $(TMP_DIR) > /dev/null; then \
		sudo umount $(TMP_DIR); \
		echo "✅ tmpfs unmounted."; \
	else \
		echo "⚠️ tmpfs is not currently mounted."; \
	fi
# Combined Flow: Builds the engine, sets up the lab, and executes the bash orchestration
run-mock: build lab-up
	@echo "🔥 Initiating Mock sysupgrade flow..."
	@bash $(SRC_BASH)/mock_upgrade.sh

# Cleans up compiled binaries, lab environment, and telemetry data
clean: lab-down
	@echo "🧹 Sweeping workspace..."
	@rm -rf $(BIN_DIR)/* $(TMP_DIR) $(FLASH_DIR) $(TELEMETRY_DIR)/*.csv
	@echo "✨ Workspace clean."

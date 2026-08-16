#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "${ROOT_DIR}"

echo "=== SyntropicOS Renode Board-Level Hardware Emulation Suite ==="

CFLAGS="-std=c99 -D_DEFAULT_SOURCE -pedantic -Wall -Wextra -I. -Isrc -Itests/unit -Itests/unit/mocks -DSYN_LOG_COLOR=1 -DSYN_USE_COREDUMP=1 -DSYN_COREDUMP_FLASH_ADDR=0 -DSYN_USE_TICKLESS=1 -DSYN_USE_DMA=1 -DSYN_USE_I2C_ASYNC=1 -DSYN_USE_SPI_ASYNC=1 -DSYN_FW_USE_HMAC=1 -DSYN_FW_USE_ED25519=1 -DSYN_USE_MULTICORE=1 -DUNITY_INCLUDE_DOUBLE -DSYN_USE_METRICS=1 -DSYN_USE_ROUTER=1 -DSYN_USE_LIN=1 -DSYN_USE_IR=1 -DSYN_USE_SMBUS=1 -DSYN_USE_PMBUS=1 -DSYN_USE_WG=1"
SRC_FILES="$(find src/syntropic -name "*.c" ! -path "*/port_stubs/*" ! -name "syn_wg.c" ! -name "syn_hpclock.c" ! -name "syn_timesync.c" ! -name "syn_lfs.c")"

echo "=== Compiling Hardware Emulation Test Drivers ==="
gcc ${CFLAGS} ${SRC_FILES} tests/unit/unity/unity.c tests/unit/mocks/mock_port.c tests/emulation/renode/test_renode_board.c -o test_renode_board -lm
gcc ${CFLAGS} ${SRC_FILES} tests/unit/unity/unity.c tests/unit/mocks/mock_port.c tests/emulation/renode/test_renode_multinode.c -o test_renode_multinode -lm
gcc ${CFLAGS} ${SRC_FILES} tests/unit/unity/unity.c tests/unit/mocks/mock_port.c tests/emulation/renode/test_renode_lfs.c -o test_renode_lfs -lm
gcc ${CFLAGS} ${SRC_FILES} tests/unit/unity/unity.c tests/unit/mocks/mock_port.c tests/emulation/renode/test_renode_display.c -o test_renode_display -lm
gcc ${CFLAGS} ${SRC_FILES} tests/unit/unity/unity.c tests/unit/mocks/mock_port.c tests/emulation/renode/test_renode_control.c -o test_renode_control -lm

echo "=== 1. W25Q64 SPI Flash & MPU6050 I2C Emulation ==="
./test_renode_board

echo "=== 2. Multi-Node Inter-MCU COBS Transport Emulation ==="
./test_renode_multinode

echo "=== 3. SPI Flash Wear-Leveled Parameter Storage Emulation ==="
./test_renode_lfs

echo "=== 4. Display Controller Canvas Framebuffer Emulation ==="
./test_renode_display

echo "=== 5. Closed-Loop ADC -> PID -> PWM Control Emulation ==="
./test_renode_control

rm -f test_renode_board test_renode_multinode test_renode_lfs test_renode_display test_renode_control
echo "=== Renode Hardware Emulation Suite Complete ==="

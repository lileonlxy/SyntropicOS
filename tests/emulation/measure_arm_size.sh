#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${ROOT_DIR}"

echo "=== ARM Cortex-M4 (Thumb-2) Size Audit for syn_dsp & syn_nn ==="
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -std=c99 -O2 -ffunction-sections -fdata-sections -fstack-usage -I. -Isrc -c src/syntropic/dsp/syn_dsp.c -o build/syn_dsp_arm.o
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -std=c99 -O2 -ffunction-sections -fdata-sections -fstack-usage -I. -Isrc -c src/syntropic/util/syn_nn.c -o build/syn_nn_arm.o

arm-none-eabi-size build/syn_dsp_arm.o build/syn_nn_arm.o
echo "=== GCC Stack Usage Analysis (.su files) ==="
cat build/syn_dsp_arm.su build/syn_nn_arm.su

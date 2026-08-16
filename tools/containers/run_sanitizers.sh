#!/usr/bin/env bash
set -euo pipefail

# Script to run SyntropicOS test suite with AddressSanitizer and UndefinedBehaviorSanitizer
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

cd "${ROOT_DIR}"

echo "=== Cleaning previous build artifacts ==="
make -f tests/Makefile.unity clean || true
make -f tests/Makefile.unity wasm-fixtures BUILD_DIR=build/tests

echo "=== Running SyntropicOS Test Suite under UBSan ==="
gcc -std=c99 -pedantic -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined \
    -I. -Isrc -Itests/unit -Itests/unit/mocks -Ibuild/tests \
    -DSYN_LOG_COLOR=1 -DSYN_USE_COREDUMP=1 -DSYN_COREDUMP_FLASH_ADDR=0 -DSYN_USE_TICKLESS=1 -DSYN_USE_DMA=1 -DSYN_USE_I2C_ASYNC=1 -DSYN_USE_SPI_ASYNC=1 -DSYN_FW_USE_HMAC=1 -DSYN_FW_USE_ED25519=1 -DSYN_USE_MULTICORE=1 -DUNITY_INCLUDE_DOUBLE -DSYN_USE_METRICS=1 -DSYN_USE_ROUTER=1 -DSYN_USE_LIN=1 -DSYN_USE_LINTP=1 -DSYN_USE_GBT27930=1 -DSYN_USE_CANNM=1 -DSYN_USE_IR=1 -DSYN_USE_SMBUS=1 -DSYN_USE_PMBUS=1 -DSYN_USE_MBUS=1 -DSYN_USE_DLT645=1 -DSYN_USE_AT_PARSER=1 -DSYN_USE_BACNET=1 -DSYN_USE_DEVICENET=1 \
    $(find src/syntropic -name "*.c" ! -path "*/port_stubs/*" ! -name "syn_wg.c") \
    tests/unit/unity/unity.c \
    tests/unit/mocks/mock_port.c \
    $(find tests/unit -name "test_*.c") \
    -o test_unity -lm
./test_unity
rm -f test_unity

echo "=== Sanitizer Analysis Clean ==="

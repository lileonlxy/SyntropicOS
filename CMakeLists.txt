cmake_minimum_required(VERSION 3.10)
project(SyntropicOS LANGUAGES C)

# ── Library target ──────────────────────────────────────────────────────────
# INTERFACE-only by default so the parent project controls compilation flags.
add_library(syntropic INTERFACE)

target_include_directories(syntropic INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── Subdirectory modules ───────────────────────────────────────────────────
add_subdirectory(src/syntropic/audio)
add_subdirectory(src/syntropic/ble)
add_subdirectory(src/syntropic/cli)
add_subdirectory(src/syntropic/control)
add_subdirectory(src/syntropic/crypto)
add_subdirectory(src/syntropic/debug)
add_subdirectory(src/syntropic/display)
add_subdirectory(src/syntropic/drivers)
add_subdirectory(src/syntropic/dsp)
add_subdirectory(src/syntropic/input)
add_subdirectory(src/syntropic/log)
add_subdirectory(src/syntropic/motor)
add_subdirectory(src/syntropic/net)
add_subdirectory(src/syntropic/output)
add_subdirectory(src/syntropic/proto)
add_subdirectory(src/syntropic/sched)
add_subdirectory(src/syntropic/sensor)
add_subdirectory(src/syntropic/storage)
add_subdirectory(src/syntropic/system)
add_subdirectory(src/syntropic/ui)
add_subdirectory(src/syntropic/util)
add_subdirectory(src/syntropic/vm)

option(CH32V307 "Build CH32V307 RISC-V port" OFF)
if(CH32V307)
    target_sources(syntropic INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/port/ch32v307/port_ch32v307.c)
    add_compile_definitions(CH32V307xx=1)
endif()

# Optional: weak stubs (link these so unimplemented port functions assert
# at runtime instead of giving a linker error)
set(SYN_STUB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/syntropic/port_stubs/syn_port_stubs.c
)

# ── Convenience target for stubs ────────────────────────────────────────────
add_library(syn_stubs INTERFACE)
target_sources(syn_stubs INTERFACE ${SYN_STUB_SOURCES})
target_link_libraries(syn_stubs INTERFACE syntropic)

# ── Tests (only when built standalone, not as a submodule) ──────────────────
# The primary test suite uses Unity (tests/Makefile.unity).
# This CMake target provides CTest integration.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_test(NAME syn_tests
             COMMAND make -f tests/Makefile.unity test
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# SyntropicOS Documentation

**High-Performance Bare-Metal Application Framework & Cooperative OS**

[![GitHub Repository](https://img.shields.io/badge/GitHub-outlookhazy%2FSyntropicOS-blue?logo=github)](https://github.com/outlookhazy/SyntropicOS)
[![Latest Release](https://img.shields.io/github/v/release/outlookhazy/SyntropicOS?logo=github)](https://github.com/outlookhazy/SyntropicOS/releases)
[![License](https://img.shields.io/github/license/outlookhazy/SyntropicOS)](https://github.com/outlookhazy/SyntropicOS/blob/main/LICENSE)

SyntropicOS is a zero-overhead, production-grade C99 framework designed for deeply embedded systems. It provides stackless multitasking, non-blocking drivers, industrial fieldbuses, and display graphics for targets ranging from 8-bit microcontrollers to 32-bit Cortex-M and RISC-V targets.

---

## Why SyntropicOS? (Design Rationale & Philosophy)

Deeply embedded systems present a fundamental architectural choice:

1. **Bare-Metal Super-Loops (`while(1)`)**: Simple to start and zero memory overhead. However, managing timers, state machines, protocol decoders, and non-blocking I/O in a single main loop quickly results in unmaintainable code. Any blocking call halts the entire microcontroller.
2. **Preemptive RTOS (FreeRTOS, Zephyr)**: Multi-tasking where every task requires an allocated stack (512 B to 4 KB RAM per task). On microcontrollers with 2 KB to 16 KB total RAM, allocating thread stacks severely constrains memory. Preemptive switches also introduce race conditions, mutex contention, context-switch overhead, and potential stack overflows.
3. **The SyntropicOS Approach**: A cooperative OS built around **stackless coroutines (protothreads `syn_pt`)**. Tasks execute sequentially on a single system stack while syntax macros handle yielding and resumption. Continuation state costs **2 bytes of RAM per thread** (`uint16_t lc`). Memory allocation is **100% static** (zero `malloc()`), preventing runtime heap fragmentation.

SyntropicOS is not the first system to use stackless coroutines — Contiki OS (created by Adam Dunkels, who invented protothreads) popularized them for microcontrollers. The difference is that **every SyntropicOS module is built from the ground up with the cooperative model in mind**. All 70+ drivers, protocol stacks, and subsystems expose non-blocking `_poll()`, `_update()`, or `_process()` APIs that yield cooperatively. There are no blocking wrappers around third-party libraries or hidden busy-waits inside module code.

---

## Architecture & Framework Comparison

| Feature | Bare-Metal Super-Loop | Preemptive RTOS (FreeRTOS / Zephyr) | SyntropicOS |
|---|---|---|---|
| **RAM per Thread** | 0 B | 512 B – 4 KB per task (stack pointer) | **2 B per thread** (`uint16_t lc`) |
| **Concurrency Model** | Manual state machines in main loop | Preemptive multi-threading | Cooperative protothreads (`syn_pt`) |
| **Memory Allocation** | Static | Dynamic heap or static pool allocation | **100% Zero-Heap / Static** |
| **Context Switch Cost** | Zero | CPU register push/pop & stack frame swap | **Zero** (C99 `switch` continuation jump) |
| **Race Conditions** | None (single thread execution) | High (requires mutexes, semaphores, spinlocks) | None across yield points |
| **Execution Safety** | Low (blocking functions freeze system) | Stack overflow risk | High (no thread stack overflow risk) |
| **Target Hardware** | 8-bit to 32-bit MCUs | 32-bit MCUs (typically >32 KB RAM) | **8-bit to 32-bit MCUs (2 KB+ RAM)** |

---

## Core Concepts Explained

### 1. Stackless Protothreads (`syn_pt`)
Protothreads provide sequential, non-blocking flow control inside standard C functions without requiring separate stacks.
- **Continuation via Duff's Device**: `PT_BEGIN()` expands to a `switch(pt->lc)` statement. When yielding (`PT_WAIT_UNTIL` or `PT_TASK_DELAY_MS`), `pt->lc` records `__LINE__` and returns `PT_WAITING`. Upon re-invocation, the switch jumps directly to the saved line.
- **RAM Footprint**: Stores only a `uint16_t` continuation variable (2 bytes RAM).
- **Variable Lifetime**: Local variables inside a protothread function do not persist across yields. Persistent state must be stored in `static` variables, global contexts, or a struct passed via `user_data`.

### 2. Cooperative Task Scheduler (`syn_sched`)
The scheduler runs an array of `SYN_Task` descriptors. On each tick, it executes the highest-priority ready task (priority `0` highest). Equal-priority tasks execute round-robin.
- **Zero Dynamic Allocation**: The application owns and allocates the `SYN_Task` array statically.
- **Tickless Idle**: Includes low-power sleep support (`syn_sched_run_tickless()`) when no tasks are ready.

### 3. Ground-Up Non-Blocking Module Ecosystem
Every driver and protocol module in SyntropicOS is written from scratch as a cooperative, non-blocking state machine. Modules expose `_poll()`, `_update()`, or `_process()` entry points (e.g. `syn_modbus_poll()`, `syn_button_update()`, `syn_ble_gatt_process_att_pdu()`) that do a bounded unit of work and return immediately. No module internally blocks, busy-waits, or calls `delay()`.

---

## Technical Specifications At-a-Glance

| Feature | Design Specification |
|---|---|
| **Concurrency** | Cooperative protothreads (`syn_pt`). Continuation state costs **2 bytes RAM** per thread. |
| **Task Scheduler** | Cooperative task runner (`syn_sched`). Task descriptors cost **~16–28 bytes RAM** per task. |
| **Memory Allocation** | **100% Zero-Heap / Static Allocation**. No `malloc()` or dynamic pool fragmentation over long runtimes. |
| **Execution Model** | All 70+ drivers & protocol stacks are written as **non-blocking state machines**. |
| **Compatibility** | Standard **C99**. Compiles with GCC, Clang, IAR, Keil, STM32CubeIDE, and Arduino IDE. |

---

## Module Documentation Index

Quick-jump to specific feature guides and API references:

### ⚡ Core & Multitasking ([Read Core Docs →](modules/multitasking.md))
- **[Protothreads (`syn_pt`)](modules/multitasking.md#1-protothreads)**: Stackless coroutines for non-blocking task execution.
- **[Task Scheduler (`syn_sched`)](modules/multitasking.md#2-cooperative-scheduler)**: Cooperative task runner with priority & delay timers.
- **[Active Objects (`syn_ao`)](modules/integration.md#2-active-object-pattern)**: FSM state machine + SPSC queue + task runner actor model.
- **[Event Flags & Mailboxes](modules/core.md)**: Thread-safe inter-task messaging and synchronization.

### 🎛️ Input / Output Drivers ([Read I/O Docs →](modules/io.md))
- **[Buttons (`syn_button`)](modules/io.md#1-button-driver)**: Debounced buttons, multi-click tap gestures, long-press, auto-repeat, and combos.
- **[Rotary Encoder (`syn_encoder`)](modules/io.md#2-rotary-encoder-driver)**: Quadrature rotary decoding and velocity tracking.
- **[LED Controller (`syn_led`)](modules/io.md#3-non-blocking-led-driver)**: Pattern blinking, flash sequences, and Morse sequences.
- **[Software PWM (`syn_soft_pwm`)](modules/io.md#4-software-pwm-driver)**: Timerless PWM generation on arbitrary GPIO pins.

### 📡 Communications & Protocol Stacks ([Read Comm Docs →](modules/communication.md))
- **[Ethernet & IP Protocol Suite (`syn_eth` / `syn_dhcp` / `syn_icmp` / `syn_autoip` / `syn_netcfg`)](modules/communication.md#6-zero-heap-ethernet--ip-protocol-suite-syn_eth-syn_dhcp-syn_icmp-syn_autoip-syn_netcfg)**: Zero-heap Ethernet II, ARP, DHCP client, ICMP Echo ping, RFC 3927 AutoIP fallback, and Link Up/Down state machine.
- **[COBS Framing (`syn_cobs`)](modules/communication.md#1-cobs--packet-router-pipeline)**: Zero-overhead `0x00`-delimited packet framing.
- **[Packet Router (`syn_router`)](modules/communication.md#1-cobs--packet-router-pipeline)**: Addressed packet dispatch (Master/Slave Node IDs) with ACKs.
- **[Industrial Modbus (`syn_modbus`)](modules/communication.md)**: Modbus RTU & Modbus TCP Master/Slave stacks.
- **[Building Automation (`syn_bacnet` / `syn_dali`)](modules/communication.md)**: BACnet MS/TP (ISO 16484-5) & DALI Lighting (IEC 62386) protocol engines.
- **[M-Bus Metering (`syn_mbus`)](modules/communication.md#2-m-bus-protocol)**: EN 13757 European utility meter bus decoder.
- **[Automotive ISO-TP & J1939](modules/communication.md)**: CAN bus multi-frame transport and heavy vehicle PGN/SPN decoder.
- **[USB 2.0 Device & Host Core (`syn_usb` / `syn_usb_host` / `syn_usb_cdc` / `syn_usb_hid`)](modules/drivers.md#9-usb-20-device-core--class-drivers-driversusb_h-usb_cdch-usb_hidh)**: Zero-heap USB 2.0 device & host core engines, CDC ACM, HID class drivers, and protothread coroutines.



### 💾 Storage & Filesystems ([Read Storage Docs →](modules/storage.md))
- **[Persistent Settings (`syn_settings`)](modules/storage.md#1-persistent-settings-manager)**: High-level configuration manager with load-or-default, change-detection & CRC-16.
- **[Flash Wear-Leveling Engine (`syn_param`)](modules/storage.md#2-flash-wear-leveling-engine-syn_paramh)**: Raw sector/page wear leveling with two-phase power-fail safety.
- **[Virtual File System (`syn_vfs`)](modules/storage.md#3-virtual-file-system-syn_vfsh)**: POSIX-like VFS abstraction for LittleFS and FAT.

### 🖥️ Display & Embedded UI ([Read Display Docs →](modules/display.md))
- **[Display Canvas (`syn_canvas`)](modules/display.md#1-framebuffer-display-canvas)**: Hardware-independent 1bpp/16bpp framebuffer & 2D graphics.
- **[Immediate-Mode GUI (`syn_imgui`)](modules/display.md#2-immediate-mode-gui)**: Zero-heap UI widgets (buttons, sliders, gauges, graphs).

### 📈 DSP & TinyML Neural Networks ([Read DSP & TinyML Docs →](modules/dsp.md))
- **[Fixed-Point Filters (`syn_filter`)](modules/dsp.md#1-digital-filters)**: Biquad lowpass/highpass, EMA, and median spike rejection.
- **[Spectral Analysis (`syn_fft` / `syn_dsp`)](modules/dsp.md#3-fast-fourier-transform--peak-detection)**: Radix-2 FFT, DCT-II, windowing, and peak tracking.
- **[TinyML Neural Networks (`syn_nn`)](modules/dsp.md#4-tinyml--fixed-point-neural-networks-utilsyn_nnh)**: Quantized 1D-CNNs, 1D Pooling, Dense layers, Self-Attention, and Protothread inference.

### 🔬 Diagnostics & System Services ([Read Debug Docs →](modules/debug.md))
- **[Lightweight Event Tracer (`syn_trace`)](modules/debug.md#1-lightweight-event-tracer)**: Timestamped circular event recorder for ISRs & tasks.
- **[Task CPU Profiler (`syn_profiler`)](modules/debug.md#2-task-cpu-profiler)**: Task CPU percentage, peak execution time, and run metrics.
- **[Serial CLI (`syn_cli`)](modules/services.md#1-interactive-serial-cli)**: Zero-allocation interactive shell with command auto-help.
- **[Software Watchdog (`syn_watchdog`)](modules/services.md#2-multi-task-software-watchdog)**: Multi-task heartbeat monitor and deadlock prevention.

---

## Getting Started & Platform Guides

- **[Getting Started Guide](getting-started.md)** — Step-by-step setup for C99 CMake & Makefile projects.
- **[IDE Integration & Setup Guides](ide-guides.md)** — Step-by-step setup for STM32CubeIDE, VS Code, Keil MDK, IAR, and Arduino.
- **[Arduino Compatibility Guide](arduino.md)** — Installing via Library Manager and working with Multi-Tab sketch examples.
- **[Porting & System Integration](porting-guide.md)** — Implementing custom GPIO, UART, and timer tick ports.


# STM32 Secure USART Reception with SHA-256 & AES-128 Example

This example demonstrates how to integrate SyntropicOS **`syn_sha256`** (pure C99 SHA-256 hash engine) and **`syn_aes`** (AES-128-CBC encryption/decryption) with UART reception on STM32 microcontrollers.

## Features

1. **USART Interrupt Reception**:
   - Collects incoming serial data streams into a `SYN_RingBuf`.

2. **SHA-256 Integrity Hashing (`syn_sha256`)**:
   - Computes a 32-byte (256-bit) cryptographic hash of every received payload.

3. **AES-128-CBC Encryption & Decryption (`syn_aes_cbc_encrypt` / `decrypt`)**:
   - Encrypts payload with PKCS#7 padding using a 128-bit key + 16-byte Initialization Vector (IV).
   - Decrypts and verifies payload integrity.

## Hardware Setup
- **Board**: STM32 Nucleo / Discovery (STM32F4 / STM32F1)
- **UART**: USART2 (115200 8N1)
- **Pins**: PA2 (TX), PA3 (RX)

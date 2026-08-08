# SyntropicOS Embedded Audio Subsystem (`src/syntropic/audio/`)

The SyntropicOS Audio Subsystem provides lightweight, zero-heap, integer-only audio codecs and PCM streaming engine for microcontrollers.

## Architecture

```text
+-----------------------+     +-----------------------+
|  IMA-ADPCM Codec      |     |  Bluetooth SBC Codec  |
|  (syn_adpcm.h/.c)     |     |  (syn_sbc.h/.c)       |
+-----------+-----------+     +-----------+-----------+
            |                             |
            +------------+  +-------------+
                         |  |
                         v  v
            +---------------------------+
            |  PCM Streaming Engine     |
            |  (syn_audio.h/.c)         |
            +-------------+-------------+
                          |
                          v
            +---------------------------+
            |  MCU I2S / SAI / DAC DMA  |
            +---------------------------+
```

## Features

### IMA-ADPCM Codec (`syn_adpcm`)
- **Compression**: 4:1 lossy compression (16-bit linear PCM <-> 4-bit ADPCM nibbles).
- **Footprint**: ~200 bytes code, ~8 bytes state (`SYN_ADPCM_State`).
- **Compatibility**: Standard WAV IMA-ADPCM format (ffmpeg, Audacity compatible).

### Bluetooth SBC Decoder (`syn_sbc`)
- **Specification**: Bluetooth SIG A2DP Sub-Band Codec spec compliant.
- **Subbands**: 4 and 8 subbands, 4/8/12/16 blocks.
- **Modes**: Mono, Dual Channel, Stereo, Joint Stereo.

### PCM Streaming Playback Engine (`syn_audio`)
- **Double-Buffering**: Ping-pong PCM buffer preventing audio underrun.
- **ISR Hooks**: Decoupled `syn_audio_isr_half()` and `syn_audio_isr_complete()` handlers.

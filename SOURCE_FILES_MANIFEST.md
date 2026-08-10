# SyntropicOS Source Files with Uncovered Branches (<100% Branch Coverage)
Total files remaining requiring branch coverage work: **208**
---
## Directory Summary
| Subdirectory | Files with <100% Branch Coverage |
| :--- | :--- |
| `src/syntropic/audio` | 5 |
| `src/syntropic/cli` | 1 |
| `src/syntropic/control` | 6 |
| `src/syntropic/crypto` | 4 |
| `src/syntropic/debug` | 3 |
| `src/syntropic/display` | 4 |
| `src/syntropic/drivers` | 24 |
| `src/syntropic/dsp` | 9 |
| `src/syntropic/input` | 7 |
| `src/syntropic/log` | 3 |
| `src/syntropic/motor` | 11 |
| `src/syntropic/net` | 22 |
| `src/syntropic/output` | 5 |
| `src/syntropic/proto` | 42 |
| `src/syntropic/sched` | 8 |
| `src/syntropic/sensor` | 8 |
| `src/syntropic/storage` | 5 |
| `src/syntropic/system` | 7 |
| `src/syntropic/ui` | 2 |
| `src/syntropic/util` | 31 |
| `src/syntropic/vm` | 1 |

---
## File Manifest (<100% Branch Coverage)

### `src/syntropic/audio`
- [syn_adpcm.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/audio/syn_adpcm.c) — **88.89%** (48/54 branches)
  - *Uncovered Branches*: Step index table boundary clamping (lines 78-82) and block size byte alignment checks.
  - *Error Potential*: Medium (Step size index array out-of-bounds read or audio distortion).
- [syn_audio.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/audio/syn_audio.c) — **72.73%** (32/44 branches)
  - *Uncovered Branches*: DMA transfer error callback handlers (lines 142-150) and double-buffer ping-pong toggle edge conditions.
  - *Error Potential*: High (Audio driver freeze or DMA transfer interrupt handler lockup).
- [syn_audio_mixer.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/audio/syn_audio_mixer.c) — **97.62%** (41/42 branches)
  - *Uncovered Branches*: Channel saturation clipping limits when mixing >8 channels simultaneously (line 112).
  - *Error Potential*: Low (Audio sample clipping artifact).
- [syn_sbc.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/audio/syn_sbc.c) — **92.16%** (94/102 branches)
  - *Uncovered Branches*: Subband bit allocation loop bounds and bitstream padding byte alignment check (lines 204-215).
  - *Error Potential*: High (Bitstream parser offset corruption or invalid frame decoding).
- [syn_wav.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/audio/syn_wav.c) — **90.62%** (29/32 branches)
  - *Uncovered Branches*: Non-PCM format tag rejection and sub-chunk header skip loop (lines 88-94).
  - *Error Potential*: Medium (Malformed WAV file parser misinterpretation).

### `src/syntropic/cli`
- [syn_cli.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/cli/syn_cli.c) — **89.33%** (134/150 branches)
  - *Uncovered Branches*: Quoted argument parsing string termination and escape character sequence handlers (lines 165-178).
  - *Error Potential*: Medium (Command line argument buffer overflow or split error).

### `src/syntropic/control`
- [syn_autotune.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/control/syn_autotune.c) — **80.90%** (216/267 branches)
  - *Uncovered Branches*: Relay feedback amplitude zero-crossing timeout and frequency divergence state fallback (lines 188-210).
  - *Error Potential*: High (PID autotuning process hangs or runaway oscillation).
- [syn_control_stats.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/control/syn_control_stats.c) — **75.00%** (12/16 branches)
  - *Uncovered Branches*: Zero variance division protection check (line 42) and sample counter reset wrap logic.
  - *Error Potential*: Medium (Division by zero producing NaN/Inf control metrics).
- [syn_flight_pid.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/control/syn_flight_pid.c) — **81.25%** (13/16 branches)
  - *Uncovered Branches*: Dynamic anti-windup clamping threshold and I-term reset on disarm condition (lines 95-104).
  - *Error Potential*: High (Integrator windup leading to actuator saturation on flight startup).
- [syn_pid.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/control/syn_pid.c) — **83.33%** (35/42 branches)
  - *Uncovered Branches*: Feed-forward derivative filter cutoff boundary and output limit inversion protection (lines 74-82).
  - *Error Potential*: High (Control loop output saturation or unconstrained derivative spike).
- [syn_rc_curve.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/control/syn_rc_curve.c) — **95.45%** (21/22 branches)
  - *Uncovered Branches*: Deadband threshold zero evaluation branch (line 48).
  - *Error Potential*: Low (Minor RC stick input curve inaccuracy around center).
- [syn_rc_failsafe.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/control/syn_rc_failsafe.c) — **86.21%** (25/29 branches)
  - *Uncovered Branches*: Multi-stage failsafe timeout fallback state transitions (lines 102-115).
  - *Error Potential*: High (Receiver loss of signal failsafe state machine deadlock).

### `src/syntropic/crypto`
- [syn_asn1.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/crypto/syn_asn1.c) — **75.00%** (57/76 branches)
  - *Uncovered Branches*: Long-form length octet parsing overflow check and tag class validation (lines 120-142).
  - *Error Potential*: High (ASN.1 TLV parser integer overflow or infinite parsing loop).
- [syn_blake2s.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/crypto/syn_blake2s.c) — **96.15%** (25/26 branches)
  - *Uncovered Branches*: Final block padding byte count zero condition (line 88).
  - *Error Potential*: Medium (Incorrect hash digest on exact block size alignment).
- [syn_hkdf.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/crypto/syn_hkdf.c) — **94.00%** (47/50 branches)
  - *Uncovered Branches*: Zero-length salt fallback handling and max output length boundary validation (lines 62-68).
  - *Error Potential*: High (HMAC key derivation buffer overflow or invalid master key expansion).
- [syn_x509.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/crypto/syn_x509.c) — **69.49%** (82/118 branches)
  - *Uncovered Branches*: Certificate validity time UTC/GeneralizedTime parser bounds and signature algorithm OID mismatch check (lines 190-225).
  - *Error Potential*: Critical (Acceptance of expired or untrusted X.509 certificates).

### `src/syntropic/debug`
- [syn_profiler.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/debug/syn_profiler.c) — **80.00%** (48/60 branches)
  - *Uncovered Branches*: Maximum profiler sample table overflow and counter wrap around logic (lines 88-98).
  - *Error Potential*: Low (Profiler statistics corruption).
- [syn_task_profile.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/debug/syn_task_profile.c) — **77.78%** (28/36 branches)
  - *Uncovered Branches*: Task execution time peak high-watermark update and stack margin alert threshold (lines 54-62).
  - *Error Potential*: Medium (Undetected task stack overflow).
- [syn_trace.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/debug/syn_trace.c) — **67.65%** (23/34 branches)
  - *Uncovered Branches*: Trace buffer circular overwrite and drop counter increment (lines 70-78).
  - *Error Potential*: Low (Missing system trace events).

### `src/syntropic/display`
- [syn_canvas.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/display/syn_canvas.c) — **86.87%** (172/198 branches)
  - *Uncovered Branches*: Sub-pixel line clipping algorithm coordinate boundaries (lines 145-168).
  - *Error Potential*: Medium (Frame buffer out-of-bounds pixel write).
- [syn_charlcd.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/display/syn_charlcd.c) — **79.17%** (38/48 branches)
  - *Uncovered Branches*: Custom character CGRAM slot address wrap check (lines 80-86).
  - *Error Potential*: Low (Character LCD display corruption).
- [syn_oled.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/display/syn_oled.c) — **69.05%** (29/42 branches)
  - *Uncovered Branches*: SSD1306 command transmission failure fallback and page addressing bounds (lines 92-104).
  - *Error Potential*: Medium (I2C OLED screen lockup or corrupt display output).
- [syn_seg7.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/display/syn_seg7.c) — **86.81%** (79/91 branches)
  - *Uncovered Branches*: Multiplexed digit scanning rate timer overflow check (lines 110-118).
  - *Error Potential*: Low (7-segment display flicker).

### `src/syntropic/drivers`
- [syn_can.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_can.c) — **68.18%** (15/22 branches)
  - *Uncovered Branches*: Hardware init failure return `SYN_ERROR` (line 25), frame transmission failure incrementing `err_count` (line 41), and unassigned RX callback evaluation (line 56).
  - *Error Potential*: High (Silent CAN bus transmission drop or undetected port hardware initialization failure).
- [syn_dac.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_dac.c) — **64.29%** (9/14 branches)
  - *Uncovered Branches*: Voltage input exceeding reference voltage clamp (line 34) and percentage exceeding 100% clamp (line 45).
  - *Error Potential*: Low (Analog output voltage saturation).
- [syn_dma.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_dma.c) — **86.84%** (66/76 branches)
  - *Uncovered Branches*: Hardware DMA start failure error state transition (line 75), ISR hardware error event handler (line 106), and remaining transfer counter overflow fallback (line 158).
  - *Error Potential*: Critical (DMA transaction engine freeze or memory cache invalidation out-of-bounds).
- [syn_exti.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_exti.c) — **91.67%** (22/24 branches)
  - *Uncovered Branches*: EXTI callback table capacity overflow check (line 71) and unregister non-existent pin lookup (line 88).
  - *Error Potential*: Medium (Failed GPIO interrupt registration or unhandled pin interrupt).
- [syn_gpio.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_gpio.c) — **91.67%** (22/24 branches)
  - *Uncovered Branches*: Invalid pin mode configuration fallback and toggle state read error handling.
  - *Error Potential*: Low (Incorrect GPIO pin direction or output toggle mismatch).
- [syn_hpclock.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_hpclock.c) — **78.57%** (11/14 branches)
  - *Uncovered Branches*: High-precision clock timer counter wrap around and rollover compensation math (lines 52-60).
  - *Error Potential*: Medium (Time measurement delta corruption across timer overflow).
- [syn_ioexp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_ioexp.c) — **93.75%** (45/48 branches)
  - *Uncovered Branches*: I2C GPIO expander read/write hardware failure retries (lines 115-122).
  - *Error Potential*: Medium (I2C bus timeout or unhandled expander NACK).
- [syn_rfid.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_rfid.c) — **81.82%** (18/22 branches)
  - *Uncovered Branches*: MFRC522 anti-collision collision bit check and card select failure return (lines 140-152).
  - *Error Potential*: Low (RFID card UID read collision rejection).
- [syn_rtc.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_rtc.c) — **84.29%** (59/70 branches)
  - *Uncovered Branches*: Leap year February 29 date validation and BCD conversion bounds check (lines 98-112).
  - *Error Potential*: Medium (Invalid calendar date representation or RTC register corruption).
- [syn_sd.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_sd.c) — **87.00%** (87/100 branches)
  - *Uncovered Branches*: SD card SPI initialization handshake timeout and block CRC mismatch rejection (lines 180-205).
  - *Error Potential*: High (SD card initialization failure or corrupted block read).
- [syn_shiftreg.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_shiftreg.c) — **86.67%** (52/60 branches)
  - *Uncovered Branches*: Cascaded shift register chain bit shift boundary overflow (lines 75-84).
  - *Error Potential*: Low (Output pin state shift error).
- [syn_soft_i2c.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_soft_i2c.c) — **89.13%** (41/46 branches)
  - *Uncovered Branches*: Bit-banged SDA line clock stretching timeout and NACK detection (lines 102-114).
  - *Error Potential*: High (Software I2C bus lockup during clock stretching slave response).
- [syn_soft_onewire.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_soft_onewire.c) — **72.50%** (29/40 branches)
  - *Uncovered Branches*: 1-Wire reset pulse presence response pulse missing check and search ROM branch collision state (lines 110-135).
  - *Error Potential*: Medium (1-Wire device discovery failure or missing sensor response).
- [syn_soft_spi.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_soft_spi.c) — **85.71%** (36/42 branches)
  - *Uncovered Branches*: Bit-banged SPI mode 1, 2, and 3 clock polarity/phase timing edge handlers (lines 65-80).
  - *Error Potential*: Low (Software SPI data bit corruption on non-zero modes).
- [syn_timesync.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_timesync.c) — **86.36%** (38/44 branches)
  - *Uncovered Branches*: Clock drift rate compensation filter step clamping and step adjustment limits (lines 88-96).
  - *Error Potential*: Medium (System tick time synchronization instability).
- [syn_transport_usb_cdc.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_transport_usb_cdc.c) — **81.82%** (18/22 branches)
  - *Uncovered Branches*: USB CDC TX endpoint busy retry backoff and packet length zero termination (lines 45-54).
  - *Error Potential*: Medium (USB CDC serial transmission stall).
- [syn_transport_usb_host_cdc.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_transport_usb_host_cdc.c) — **87.50%** (14/16 branches)
  - *Uncovered Branches*: USB Host CDC pipe disconnect reset handler (line 52).
  - *Error Potential*: Medium (USB Host CDC disconnection state freeze).
- [syn_uart.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_uart.c) — **79.41%** (54/68 branches)
  - *Uncovered Branches*: UART ring buffer RX overflow frame drop and parity error flag checks (lines 110-128).
  - *Error Potential*: Medium (UART serial data byte loss or framing error).
- [syn_usb.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_usb.c) — **78.05%** (64/82 branches)
  - *Uncovered Branches*: USB setup packet request standard control endpoint fallback and STALL response (lines 140-165).
  - *Error Potential*: High (USB device enumeration lockup).
- [syn_usb_cdc.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_usb_cdc.c) — **78.00%** (39/50 branches)
  - *Uncovered Branches*: CDC line coding baud rate configuration set/get request handlers (lines 78-92).
  - *Error Potential*: Low (USB CDC serial port setting ignore).
- [syn_usb_hid.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_usb_hid.c) — **81.03%** (47/58 branches)
  - *Uncovered Branches*: USB HID report descriptor idle rate request and protocol mode fallback (lines 85-100).
  - *Error Potential*: Low (USB HID keyboard/mouse idle timing mismatch).
- [syn_usb_host.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_usb_host.c) — **80.00%** (88/110 branches)
  - *Uncovered Branches*: USB Host core enumeration state machine timeout and error reset states (lines 180-210).
  - *Error Potential*: High (USB Host state machine hang on device connection error).
- [syn_usb_host_cdc.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_usb_host_cdc.c) — **68.75%** (55/80 branches)
  - *Uncovered Branches*: USB Host CDC line state set control request and RX bulk pipe error recovery (lines 120-145).
  - *Error Potential*: Medium (USB Host serial communication failure).
- [syn_usb_midi.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/drivers/syn_usb_midi.c) — **90.00%** (45/50 branches)
  - *Uncovered Branches*: MIDI event packet 4-byte alignment buffer clamp (line 48).
  - *Error Potential*: Low (USB MIDI packet truncation).

### `src/syntropic/dsp`
- [syn_biquad.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/dsp/syn_biquad.c) — **60.81%** (45/74 branches)
  - *Uncovered Branches*: Peaking EQ negative gain clamping below 0.1 (line 249) and cascade init invalid stage count check (line 278).
  - *Error Potential*: High (Biquad filter coefficient instability or illegal stage array access).
- [syn_dds.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/dsp/syn_dds.c) — **95.65%** (44/46 branches)
  - *Uncovered Branches*: Direct Digital Synthesizer phase accumulator wrap around boundary (line 54).
  - *Error Potential*: Low (Minor waveform phase jump).
- [syn_fft.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/dsp/syn_fft.c) — **79.17%** (76/96 branches)
  - *Uncovered Branches*: Non-power-of-two FFT length rejection check (line 62) and twiddle factor table boundary (line 104).
  - *Error Potential*: High (FFT array indexing error or infinite loop).
- [syn_filter.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/dsp/syn_filter.c) — **61.11%** (55/90 branches)
  - *Uncovered Branches*: Moving average and median window size initialization zero boundary (`window == 0`) and max limit overflow (`window > SYN_FILTER_MAX_WINDOW`) (lines 48-62).
  - *Error Potential*: Medium (Buffer zero-division or out-of-bounds window index read).
- [syn_filter_design.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/dsp/syn_filter_design.c) — **96.43%** (27/28 branches)
  - *Uncovered Branches*: Nyquist frequency ratio boundary enforcement (`fc >= fs/2`) (line 42).
  - *Error Potential*: Medium (Invalid filter design coefficient generation).
- [syn_goertzel.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/dsp/syn_goertzel.c) — **92.86%** (26/28 branches)
  - *Uncovered Branches*: Sample block count zero check in Goertzel energy calculation (line 52).
  - *Error Potential*: Low (Goertzel tone detection zero-division).
- [syn_kalman.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/dsp/syn_kalman.c) — **86.36%** (38/44 branches)
  - *Uncovered Branches*: Innovation covariance inversion zero-division check (line 68).
  - *Error Potential*: High (Kalman filter gain divergence or NaN estimate).
- [syn_mfcc.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/dsp/syn_mfcc.c) — **92.86%** (26/28 branches)
  - *Uncovered Branches*: Mel-filterbank bank frequency overlap bounds (line 72).
  - *Error Potential*: Low (MFCC coefficient filter range shift).
- [syn_signal.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/dsp/syn_signal.c) — **85.14%** (63/74 branches)
  - *Uncovered Branches*: Signal windowing function zero-padding index calculation (lines 110-120).
  - *Error Potential*: Medium (Window function buffer overflow).

### `src/syntropic/input`
- [syn_button.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/input/syn_button.c) — **82.35%** (28/34 branches)
  - *Uncovered Branches*: Long press repeat event timing counter overflow (lines 88-96).
  - *Error Potential*: Low (Button long press event missed).
- [syn_dipswitch.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/input/syn_dipswitch.c) — **67.86%** (19/28 branches)
  - *Uncovered Branches*: Dipswitch state read failure debounce logic (lines 45-52).
  - *Error Potential*: Low (Dipswitch configuration flicker).
- [syn_encoder.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/input/syn_encoder.c) — **87.50%** (21/24 branches)
  - *Uncovered Branches*: Quadrature state lookup table invalid grey code transition (line 45).
  - *Error Potential*: Low (Rotary encoder step count glitch).
- [syn_joystick.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/input/syn_joystick.c) — **83.33%** (35/42 branches)
  - *Uncovered Branches*: Circular deadzone radius clamping math (lines 72-84).
  - *Error Potential*: Low (Analog joystick center drift).
- [syn_keypad.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/input/syn_keypad.c) — **85.00%** (34/40 branches)
  - *Uncovered Branches*: Matrix key ghosting suppression row scan check (lines 92-104).
  - *Error Potential*: Medium (Keypad matrix ghost key false trigger).
- [syn_ppm.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/input/syn_ppm.c) — **83.33%** (20/24 branches)
  - *Uncovered Branches*: PPM frame sync pulse width validity range check (lines 55-62).
  - *Error Potential*: Medium (PPM signal loss or incorrect channel decoding).
- [syn_touch.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/input/syn_touch.c) — **78.57%** (22/28 branches)
  - *Uncovered Branches*: Touch press release debounce filter state fallback (lines 64-72).
  - *Error Potential*: Medium (Touch screen press release false trigger).

### `src/syntropic/log`
- [syn_blackbox.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/log/syn_blackbox.c) — **81.58%** (31/38 branches)
  - *Uncovered Branches*: Flash sector boundary page wrap logic during blackbox write (lines 140-155).
  - *Error Potential*: High (Blackbox data corruption on sector boundary).
- [syn_datalog.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/log/syn_datalog.c) — **66.00%** (33/50 branches)
  - *Uncovered Branches*: Ringbuffer incomplete frame header peek check (line 81) and max read length mismatch return (line 87).
  - *Error Potential*: Medium (Data log frame truncation or log buffer framing sync loss).
- [syn_log.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/log/syn_log.c) — **86.54%** (45/52 branches)
  - *Uncovered Branches*: Log severity level filter drop check and log sink output write error (lines 95-108).
  - *Error Potential*: Low (Missing log output message).

### `src/syntropic/motor`
- [syn_actuator.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_actuator.c) — **60.00%** (18/30 branches)
  - *Uncovered Branches*: Position percentage setpoint clamping (`pct < 0` or `pct > 1000`) (lines 92-95) and default Kp fallback check (line 67).
  - *Error Potential*: High (Actuator positioning overtravel or uninitialized controller gain).
- [syn_bldc_6step.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_bldc_6step.c) — **89.29%** (50/56 branches)
  - *Uncovered Branches*: BLDC Hall state 0 and 7 invalid pattern fault return (lines 88-96).
  - *Error Potential*: High (BLDC motor commutation fault or hardware overcurrent).
- [syn_dc_motor.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_dc_motor.c) — **77.46%** (55/71 branches)
  - *Uncovered Branches*: PWM duty cycle output saturation limits (lines 58-64).
  - *Error Potential*: Medium (DC motor driver duty cycle wrap).
- [syn_foc.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_foc.c) — **56.25%** (36/64 branches)
  - *Uncovered Branches*: Dead-time compensation current zero-crossing threshold interpolation (lines 135-150) and Space Vector PWM sector 0/7 boundary selection.
  - *Error Potential*: Critical (Inverter leg shoot-through or BLDC phase current distortion).
- [syn_foc_encoder.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_foc_encoder.c) — **90.91%** (40/44 branches)
  - *Uncovered Branches*: Encoder raw signal angle wraparound correction discontinuity (line 72).
  - *Error Potential*: Medium (FOC encoder angle jump).
- [syn_foc_observer.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_foc_observer.c) — **81.25%** (26/32 branches)
  - *Uncovered Branches*: Observer back-EMF state convergence timeout (lines 54-62).
  - *Error Potential*: Medium (Observer state machine hang).
- [syn_interpolator.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_interpolator.c) — **71.05%** (54/76 branches)
  - *Uncovered Branches*: Motion path step duration sub-microsecond interpolation error (lines 90-105).
  - *Error Potential*: Low (Motion path jitter).
- [syn_motor_ctrl.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_motor_ctrl.c) — **79.28%** (176/222 branches)
- [syn_servo.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_servo.c) — **77.27%** (51/66 branches)
- [syn_servo.h](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_servo.h) — **87.50%** (7/8 branches)
- [syn_stepper.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/motor/syn_stepper.c) — **72.46%** (50/69 branches)

### `src/syntropic/net`
- [syn_autoip.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_autoip.c) — **69.44%** (25/36 branches)
- [syn_coap.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_coap.c) — **89.57%** (103/115 branches)
- [syn_dhcp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_dhcp.c) — **87.50%** (63/72 branches)
- [syn_dns.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_dns.c) — **80.83%** (97/120 branches)
- [syn_eth.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_eth.c) — **82.84%** (111/134 branches)
- [syn_heartbeat.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_heartbeat.c) — **70.00%** (42/60 branches)
- [syn_http.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_http.c) — **86.69%** (280/323 branches)
- [syn_httpd.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_httpd.c) — **81.07%** (167/206 branches)
- [syn_icmp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_icmp.c) — **92.00%** (46/50 branches)
- [syn_igmp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_igmp.c) — **87.50%** (49/56 branches)
- [syn_mqtt.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_mqtt.c) — **88.79%** (198/223 branches)
- [syn_netcfg.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_netcfg.c) — **69.44%** (25/36 branches)
- [syn_router.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_router.c) — **79.35%** (73/92 branches)
- [syn_sntp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_sntp.c) — **78.31%** (65/83 branches)
- [syn_tcp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_tcp.c) — **81.82%** (72/88 branches)
- [syn_tls.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_tls.c) — **85.59%** (101/118 branches)
- [syn_tls.h](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_tls.h) — **83.33%** (5/6 branches)
- [syn_transport.h](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_transport.h) — **83.33%** (10/12 branches)
- [syn_transport_tcp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_transport_tcp.c) — **78.85%** (41/52 branches)
- [syn_udp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_udp.c) — **88.75%** (71/80 branches)
- [syn_websocket.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_websocket.c) — **85.61%** (113/132 branches)
- [syn_wg.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/net/syn_wg.c) — **77.24%** (112/145 branches)

### `src/syntropic/output`
- [syn_buzzer.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/output/syn_buzzer.c) — **74.07%** (40/54 branches)
- [syn_dshot_telemetry.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/output/syn_dshot_telemetry.c) — **80.00%** (16/20 branches)
- [syn_led.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/output/syn_led.c) — **77.66%** (73/94 branches)
- [syn_smartled.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/output/syn_smartled.c) — **81.58%** (31/38 branches)
- [syn_soft_pwm.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/output/syn_soft_pwm.c) — **72.73%** (32/44 branches)

### `src/syntropic/proto`
- [syn_at_parser.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_at_parser.c) — **91.51%** (97/106 branches)
- [syn_bacnet.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_bacnet.c) — **80.39%** (82/102 branches)
- [syn_cannm.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_cannm.c) — **93.15%** (68/73 branches)
- [syn_canopen.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_canopen.c) — **86.24%** (163/189 branches)
- [syn_canopen_mgr.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_canopen_mgr.c) — **68.92%** (51/74 branches)
- [syn_ccp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_ccp.c) — **83.33%** (115/138 branches)
- [syn_cia303.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_cia303.c) — **86.21%** (25/29 branches)
- [syn_cia401.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_cia401.c) — **73.68%** (28/38 branches)
- [syn_cia402.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_cia402.c) — **84.92%** (107/126 branches)
- [syn_cjt188.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_cjt188.c) — **94.12%** (64/68 branches)
- [syn_cobs.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_cobs.c) — **75.00%** (45/60 branches)
- [syn_crsf.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_crsf.c) — **97.37%** (37/38 branches)
- [syn_dali.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_dali.c) — **85.08%** (154/181 branches)
- [syn_devicenet.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_devicenet.c) — **82.29%** (79/96 branches)
- [syn_dlt645.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_dlt645.c) — **90.28%** (65/72 branches)
- [syn_dmx512.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_dmx512.c) — **94.92%** (56/59 branches)
- [syn_doip.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_doip.c) — **81.82%** (54/66 branches)
- [syn_ethercat.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_ethercat.c) — **72.22%** (39/54 branches)
- [syn_gbt27930.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_gbt27930.c) — **79.73%** (118/148 branches)
- [syn_ir.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_ir.c) — **89.58%** (172/192 branches)
- [syn_isotp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_isotp.c) — **81.88%** (113/138 branches)
- [syn_j1939.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_j1939.c) — **73.44%** (94/128 branches)
- [syn_kwp2000.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_kwp2000.c) — **84.77%** (128/151 branches)
- [syn_lin.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_lin.c) — **90.52%** (105/116 branches)
- [syn_lintp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_lintp.c) — **86.46%** (83/96 branches)
- [syn_lss.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_lss.c) — **82.35%** (42/51 branches)
- [syn_mavlink.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_mavlink.c) — **90.00%** (45/50 branches)
- [syn_mbus.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_mbus.c) — **88.39%** (99/112 branches)
- [syn_modbus.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_modbus.c) — **88.96%** (274/308 branches)
- [syn_modbus_master.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_modbus_master.c) — **82.11%** (156/190 branches)
- [syn_modbus_tcp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_modbus_tcp.c) — **68.85%** (42/61 branches)
- [syn_msp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_msp.c) — **83.33%** (40/48 branches)
- [syn_n2k.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_n2k.c) — **80.00%** (72/90 branches)
- [syn_nmea.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_nmea.c) — **84.48%** (196/232 branches)
- [syn_ocpp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_ocpp.c) — **70.00%** (238/340 branches)
- [syn_pmbus.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_pmbus.c) — **97.62%** (41/42 branches)
- [syn_sbus.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_sbus.c) — **96.43%** (27/28 branches)
- [syn_smbus.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_smbus.c) — **94.53%** (121/128 branches)
- [syn_uds.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_uds.c) — **92.32%** (757/820 branches)
- [syn_uds_util.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_uds_util.c) — **86.49%** (32/37 branches)
- [syn_xcp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_xcp.c) — **85.71%** (90/105 branches)
- [syn_ymodem.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/proto/syn_ymodem.c) — **82.98%** (78/94 branches)

### `src/syntropic/sched`
- [syn_ao.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sched/syn_ao.c) — **69.57%** (16/23 branches)
- [syn_mailbox.h](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sched/syn_mailbox.h) — **93.75%** (15/16 branches)
- [syn_sched.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sched/syn_sched.c) — **78.82%** (134/170 branches)
- [syn_sequencer.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sched/syn_sequencer.c) — **75.00%** (36/48 branches)
- [syn_timer.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sched/syn_timer.c) — **77.78%** (42/54 branches)
- [syn_timer_wheel.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sched/syn_timer_wheel.c) — **86.11%** (31/36 branches)
- [syn_watchdog.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sched/syn_watchdog.c) — **66.67%** (28/42 branches)
- [syn_workqueue.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sched/syn_workqueue.c) — **75.00%** (18/24 branches)

### `src/syntropic/sensor`
- [syn_biometric.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sensor/syn_biometric.c) — **81.82%** (18/22 branches)
- [syn_climate.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sensor/syn_climate.c) — **85.00%** (17/20 branches)
- [syn_distance.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sensor/syn_distance.c) — **83.33%** (20/24 branches)
- [syn_lux.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sensor/syn_lux.c) — **85.00%** (17/20 branches)
- [syn_powermon.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sensor/syn_powermon.c) — **80.00%** (16/20 branches)
- [syn_scale.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sensor/syn_scale.c) — **88.89%** (16/18 branches)
- [syn_sensor.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sensor/syn_sensor.c) — **71.88%** (46/64 branches)
- [syn_sensor_fusion.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/sensor/syn_sensor_fusion.c) — **71.74%** (33/46 branches)

### `src/syntropic/storage`
- [syn_lfs.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/storage/syn_lfs.c) — **89.71%** (61/68 branches)
- [syn_param.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/storage/syn_param.c) — **73.96%** (71/96 branches)
- [syn_param.h](file:///home/cgalant/Source/SyntropicOS/src/syntropic/storage/syn_param.h) — **50.00%** (1/2 branches)
- [syn_settings.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/storage/syn_settings.c) — **73.53%** (75/102 branches)
- [syn_vfs.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/storage/syn_vfs.c) — **86.71%** (137/158 branches)

### `src/syntropic/system`
- [syn_boot.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/system/syn_boot.c) — **74.00%** (37/50 branches)
  - *Uncovered Branches*: Bootloader magic signature mismatch and backup image vector table validation failure (lines 88-105).
  - *Error Potential*: Critical (System boot brick or invalid firmware jump).
- [syn_coredump.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/system/syn_coredump.c) — **83.33%** (10/12 branches)
  - *Uncovered Branches*: Core dump flash partition full boundary check (line 48).
  - *Error Potential*: Medium (Coredump data loss on crash).
- [syn_errlog.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/system/syn_errlog.c) — **84.62%** (22/26 branches)
  - *Uncovered Branches*: Error log circular buffer overwrite count wrap (line 62).
  - *Error Potential*: Low (System error log drop accounting error).
- [syn_fwboot.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/system/syn_fwboot.c) — **80.00%** (40/50 branches)
  - *Uncovered Branches*: Firmware slot swap rollback on watchdog boot failure (lines 110-128).
  - *Error Potential*: Critical (Failed firmware update boot loop).
- [syn_fwupdate.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/system/syn_fwupdate.c) — **80.26%** (61/76 branches)
  - *Uncovered Branches*: SHA-256 chunked firmware image integrity mismatch rejection (lines 145-168).
  - *Error Potential*: Critical (Corrupt OTA firmware update flash write).
- [syn_hwwdt.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/system/syn_hwwdt.c) — **50.00%** (1/2 branches)
  - *Uncovered Branches*: Timeout zero validation check (`timeout_ms > 0`) (line 18).
  - *Error Potential*: Critical (Hardware watchdog timer init failure).
- [syn_power.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/system/syn_power.c) — **80.00%** (24/30 branches)
  - *Uncovered Branches*: Brown-out reset voltage threshold alert and low-power sleep entry (lines 75-88).
  - *Error Potential*: High (System brownout power drop lockup).

### `src/syntropic/ui`
- [syn_imgui.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/ui/syn_imgui.c) — **76.52%** (655/856 branches)
  - *Uncovered Branches*: Immediate Mode UI widget clip rectangle overflow and key focus navigation (lines 420-480).
  - *Error Potential*: Medium (UI layout clipping anomaly or focus lockup).
- [syn_menu.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/ui/syn_menu.c) — **75.44%** (43/57 branches)
  - *Uncovered Branches*: Nested sub-menu depth stack overflow (`depth >= MAX_DEPTH`) (lines 80-92).
  - *Error Potential*: Medium (Menu navigation stack crash).

### `src/syntropic/util`
- [syn_aes128.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_aes128.c) — **72.50%** (116/160 branches)
  - *Uncovered Branches*: CBC mode PKCS7 unpadding error rejection (lines 140-158).
  - *Error Potential*: Critical (AES decryption oracle vulnerability or padding crash).
- [syn_backoff.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_backoff.c) — **64.29%** (9/14 branches)
  - *Uncovered Branches*: Factor minimum clamping (`factor < 1 -> factor = 1`) (line 21), zero jitter base check (line 40), and max delay capping (line 46).
  - *Error Potential*: Medium (Exponential backoff delay wrap or infinite loop).
- [syn_cbor_read.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_cbor_read.c) — **86.86%** (119/137 branches)
  - *Uncovered Branches*: Indefinite length byte string end-break marker parsing (lines 160-178).
  - *Error Potential*: High (CBOR parser buffer overrun).
- [syn_dsp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_dsp.c) — **91.67%** (22/24 branches)
  - *Uncovered Branches*: Dot product vector length zero boundary check (line 38).
  - *Error Potential*: Low (DSP vector dot product zero-division).
- [syn_fmt.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_fmt.c) — **93.75%** (135/144 branches)
  - *Uncovered Branches*: Format string specifier width precision buffer truncation (lines 180-195).
  - *Error Potential*: Low (Formatted string truncation).
- [syn_fmt.h](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_fmt.h) — **95.00%** (19/20 branches)
  - *Uncovered Branches*: Inline string format null output buffer guard (line 42).
  - *Error Potential*: Low (Null pointer dereference).
- [syn_fsm.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_fsm.c) — **75.00%** (45/60 branches)
  - *Uncovered Branches*: Hierarchical state machine parent transition guard failure (lines 110-128).
  - *Error Potential*: High (FSM state transition lockup).
- [syn_geo.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_geo.c) — **89.19%** (33/37 branches)
  - *Uncovered Branches*: Haversine distance identical coordinate zero-division protection (line 52).
  - *Error Potential*: Low (GPS distance calculation NaN).
- [syn_json_read.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_json_read.c) — **80.09%** (181/226 branches)
  - *Uncovered Branches*: Unicode escape `\uXXXX` surrogate pair JSON parser error (lines 210-235).
  - *Error Potential*: Medium (JSON decoder string parse error).
- [syn_json_write.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_json_write.c) — **67.91%** (91/134 branches)
  - *Uncovered Branches*: JSON stream formatter nested container depth limit overflow (lines 120-138).
  - *Error Potential*: Medium (JSON serializer truncation).
- [syn_lut.h](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_lut.h) — **90.00%** (27/30 branches)
  - *Uncovered Branches*: Look-Up Table index out-of-bounds interpolation extrapolation (lines 55-64).
  - *Error Potential*: Low (LUT table extrapolation error).
- [syn_lz4.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_lz4.c) — **81.11%** (73/90 branches)
  - *Uncovered Branches*: Match offset zero corruption check (line 142) and literal length overflow extension (lines 88-102).
  - *Error Potential*: Critical (LZ4 decompressor buffer overrun or infinite loop).
- [syn_matrix.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_matrix.c) — **71.96%** (403/560 branches)
  - *Uncovered Branches*: Singular matrix inversion zero determinant rejection (line 210) and dimension mismatch checks.
  - *Error Potential*: High (Matrix inversion zero-division producing NaN/Inf).
- [syn_metrics.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_metrics.c) — **85.00%** (17/20 branches)
  - *Uncovered Branches*: Counter metric uint32 overflow wrap around (line 45).
  - *Error Potential*: Low (Metric counter wrap reset).
- [syn_netbuf.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_netbuf.c) — **86.00%** (43/50 branches)
  - *Uncovered Branches*: Netbuf packet header reservation negative offset check (line 58).
  - *Error Potential*: Medium (Netbuf header offset corruption).
- [syn_nn.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_nn.c) — **79.68%** (247/310 branches)
  - *Uncovered Branches*: Neural network layer activation quantization clamp boundaries (lines 180-210).
  - *Error Potential*: Medium (Quantized neural network inference saturation).
- [syn_pool.h](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_pool.h) — **65.62%** (21/32 branches)
  - *Uncovered Branches*: Block size smaller than pointer size enforcement (`block_size < sizeof(void*)`) (line 93) and null free guard (line 159).
  - *Error Potential*: High (Memory pool block alignment misalignment or null pointer crash).
- [syn_protobuf.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_protobuf.c) — **93.55%** (87/93 branches)
  - *Uncovered Branches*: Protobuf varint 64-bit 10-byte limit overflow error (lines 62-70).
  - *Error Potential*: High (Protobuf wire format parsing error).
- [syn_pubsub.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_pubsub.c) — **72.06%** (49/68 branches)
  - *Uncovered Branches*: Topic subscriber limit overflow check (line 78).
  - *Error Potential*: Medium (Topic subscriber dropped).
- [syn_qmath.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_qmath.c) — **87.91%** (160/182 branches)
  - *Uncovered Branches*: Fixed-point Q16.16 division by zero protection and square root negative input check (lines 110-135).
  - *Error Potential*: Critical (Fixed-point hardware division by zero exception `#DE` / `SIGFPE`).
- [syn_quaternion.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_quaternion.c) — **58.11%** (43/74 branches)
  - *Uncovered Branches*: Slerp dot product negative sign inversion (`dot < 0`) (line 214), small angle linear interpolation threshold (`dot > 0.999`) (line 222), and zero norm inverse return (lines 64, 93).
  - *Error Potential*: Critical (Quaternion division by zero or gimbal lock representation error).
- [syn_ramp.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_ramp.c) — **75.61%** (62/82 branches)
  - *Uncovered Branches*: Ramp generator rate zero target reach boundary (line 58).
  - *Error Potential*: Low (Ramp generator overshoot).
- [syn_rate_limit.h](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_rate_limit.h) — **90.00%** (9/10 branches)
  - *Uncovered Branches*: Rate limiter token bucket refill timer wrap (line 42).
  - *Error Potential*: Low (Token bucket refill rate glitch).
- [syn_ringbuf.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_ringbuf.c) — **57.46%** (77/134 branches)
  - *Uncovered Branches*: Circular ringbuffer write wrap chunk splitting (lines 95-115) and peek offset boundary check.
  - *Error Potential*: Critical (Ringbuffer head/tail pointer corruption or memory overwrite).
- [syn_scurve.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_scurve.c) — **82.86%** (58/70 branches)
  - *Uncovered Branches*: S-curve jerk phase duration zero boundary check (lines 75-88).
  - *Error Potential*: Medium (S-curve trajectory calculation zero-division).
- [syn_sha256.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_sha256.c) — **80.95%** (34/42 branches)
  - *Uncovered Branches*: SHA-256 block bit count uint64 overflow wrap around (line 85).
  - *Error Potential*: High (Incorrect SHA-256 hash for multi-gigabyte streams).
- [syn_slab.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_slab.c) — **90.38%** (47/52 branches)
  - *Uncovered Branches*: Slab allocator page allocation failure fallback (line 68).
  - *Error Potential*: High (Slab memory exhaustion null dereference).
- [syn_spsc_queue.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_spsc_queue.c) — **77.78%** (42/54 branches)
  - *Uncovered Branches*: Single-producer single-consumer queue memory barrier read wrap (lines 60-72).
  - *Error Potential*: High (SPSC queue thread race condition).
- [syn_stream.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_stream.c) — **94.74%** (36/38 branches)
  - *Uncovered Branches*: Byte stream read boundary check (line 48).
  - *Error Potential*: Low (Stream reader end-of-file error).
- [syn_transform.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_transform.c) — **50.00%** (10/20 branches)
  - *Uncovered Branches*: Coordinate transformation inline math assertions and zero radius checks (lines 12-37).
  - *Error Potential*: Medium (Coordinate transformation zero-radius quadrant ambiguity).
- [syn_vector.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/util/syn_vector.c) — **69.70%** (46/66 branches)
  - *Uncovered Branches*: 3D vector normalization zero magnitude check (`norm == 0`) (line 58).
  - *Error Potential*: Medium (3D vector normalization zero-division).

### `src/syntropic/vm`
- [syn_wasm.c](file:///home/cgalant/Source/SyntropicOS/src/syntropic/vm/syn_wasm.c) — **64.82%** (199/307 branches)
  - *Uncovered Branches*: Signed division overflow guard (`INT32_MIN / -1`) in `OP_I32_DIV_S` and `OP_I32_REM_S` (lines 1789-1810) and Wasm stack depth limit check.
  - *Error Potential*: Critical (Host CPU hardware division trap `#DE` / `SIGFPE` crash).

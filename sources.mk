# SyntropicOS sources — include this from your Makefile
#
# Usage:
#   SYN_DIR := lib/SyntropicOS
#   include $(SYN_DIR)/sources.mk
#   CFLAGS += -I$(SYN_INC)
#   SRCS   += $(SYN_SRCS)
#
# To also include weak stubs (recommended):
#   SRCS += $(SYN_STUB_SRCS)

SYN_SRCS := \
	$(SYN_DIR)/src/syntropic/drivers/syn_gpio.c \
	$(SYN_DIR)/src/syntropic/drivers/syn_uart.c \
	$(SYN_DIR)/src/syntropic/util/syn_ringbuf.c \
	$(SYN_DIR)/src/syntropic/util/syn_fsm.c \
	$(SYN_DIR)/src/syntropic/util/syn_crc.c \
	$(SYN_DIR)/src/syntropic/sched/syn_sched.c \
	$(SYN_DIR)/src/syntropic/sched/syn_timer.c \
	$(SYN_DIR)/src/syntropic/sched/syn_watchdog.c \
	$(SYN_DIR)/src/syntropic/log/syn_log.c \
	$(SYN_DIR)/src/syntropic/cli/syn_cli.c \
	$(SYN_DIR)/src/syntropic/input/syn_button.c \
	$(SYN_DIR)/src/syntropic/output/syn_led.c \
	$(SYN_DIR)/src/syntropic/output/syn_soft_pwm.c \
	$(SYN_DIR)/src/syntropic/dsp/syn_filter.c \
	$(SYN_DIR)/src/syntropic/control/syn_pid.c \
	$(SYN_DIR)/src/syntropic/motor/syn_stepper.c \
	$(SYN_DIR)/src/syntropic/motor/syn_servo.c \
	$(SYN_DIR)/src/syntropic/motor/syn_dc_motor.c \
	$(SYN_DIR)/src/syntropic/motor/syn_motor_ctrl.c \
	$(SYN_DIR)/src/syntropic/proto/syn_at_parser.c \
	$(SYN_DIR)/src/syntropic/proto/syn_cobs.c \
	$(SYN_DIR)/src/syntropic/proto/syn_smbus.c \
	$(SYN_DIR)/src/syntropic/proto/syn_pmbus.c \
	$(SYN_DIR)/src/syntropic/proto/syn_mbus.c \
	$(SYN_DIR)/src/syntropic/sensor/syn_sensor.c \
	$(SYN_DIR)/src/syntropic/storage/syn_param.c \
	$(SYN_DIR)/src/syntropic/input/syn_encoder.c \
	$(SYN_DIR)/src/syntropic/drivers/syn_adc.c \
	$(SYN_DIR)/src/syntropic/sched/syn_sequencer.c \
	$(SYN_DIR)/src/syntropic/proto/syn_modbus.c \
	$(SYN_DIR)/src/syntropic/debug/syn_trace.c \
	$(SYN_DIR)/src/syntropic/debug/syn_profiler.c \
	$(SYN_DIR)/src/syntropic/system/syn_boot.c \
	$(SYN_DIR)/src/syntropic/system/syn_errlog.c \
	$(SYN_DIR)/src/syntropic/util/syn_fmt.c \
	$(SYN_DIR)/src/syntropic/sched/syn_workqueue.c \
	$(SYN_DIR)/src/syntropic/drivers/syn_exti.c \
	$(SYN_DIR)/src/syntropic/dsp/syn_signal.c \
	$(SYN_DIR)/src/syntropic/util/syn_ramp.c \
	$(SYN_DIR)/src/syntropic/system/syn_power.c \
	$(SYN_DIR)/src/syntropic/motor/syn_actuator.c \
	$(SYN_DIR)/src/syntropic/display/syn_canvas.c \
	$(SYN_DIR)/src/syntropic/ui/syn_menu.c \
	$(SYN_DIR)/src/syntropic/ui/syn_imgui.c \
	$(SYN_DIR)/src/syntropic/drivers/syn_can.c \
	$(SYN_DIR)/src/syntropic/net/syn_router.c \
	$(SYN_DIR)/src/syntropic/net/syn_heartbeat.c \
	$(SYN_DIR)/src/syntropic/sched/syn_ao.c \
	$(SYN_DIR)/src/syntropic/storage/syn_vfs.c \
	$(SYN_DIR)/src/syntropic/storage/syn_lfs.c \
	$(SYN_DIR)/src/syntropic/net/syn_coap.c \
	$(SYN_DIR)/src/syntropic/dsp/syn_biquad.c \
	$(SYN_DIR)/src/syntropic/dsp/syn_fft.c \
	$(SYN_DIR)/src/syntropic/system/syn_fault.c \
	$(SYN_DIR)/src/syntropic/control/syn_autotune.c \
	$(SYN_DIR)/src/syntropic/storage/syn_settings.c \
	$(SYN_DIR)/src/syntropic/log/syn_datalog.c \
	$(SYN_DIR)/src/syntropic/util/syn_scurve.c \
	$(SYN_DIR)/src/syntropic/util/syn_pubsub.c \
	$(SYN_DIR)/src/syntropic/util/syn_sha256.c \
	$(SYN_DIR)/src/syntropic/system/syn_coredump.c \
	$(SYN_DIR)/src/syntropic/crypto/syn_blake2s.c \
	$(SYN_DIR)/src/syntropic/crypto/syn_chacha20poly1305.c \
	$(SYN_DIR)/src/syntropic/crypto/syn_x25519.c \
	$(SYN_DIR)/src/syntropic/net/syn_sntp.c \
	$(SYN_DIR)/src/syntropic/net/syn_wg.c \
	$(SYN_DIR)/src/syntropic/output/syn_buzzer.c \
	$(SYN_DIR)/src/syntropic/input/syn_keypad.c \
	$(SYN_DIR)/src/syntropic/input/syn_touch.c \
	$(SYN_DIR)/src/syntropic/input/syn_dipswitch.c \
	$(SYN_DIR)/src/syntropic/display/syn_charlcd.c \
	$(SYN_DIR)/src/syntropic/display/syn_oled.c \
	$(SYN_DIR)/src/syntropic/display/syn_seg7.c \
	$(SYN_DIR)/src/syntropic/drivers/syn_shiftreg.c \
	$(SYN_DIR)/src/syntropic/drivers/syn_ioexp.c \
	$(SYN_DIR)/src/syntropic/input/syn_joystick.c \
	$(SYN_DIR)/src/syntropic/sensor/syn_powermon.c \
	$(SYN_DIR)/src/syntropic/sensor/syn_climate.c \
	$(SYN_DIR)/src/syntropic/output/syn_smartled.c \
	$(SYN_DIR)/src/syntropic/sensor/syn_distance.c \
	$(SYN_DIR)/src/syntropic/sensor/syn_scale.c \
	$(SYN_DIR)/src/syntropic/sensor/syn_lux.c \
	$(SYN_DIR)/src/syntropic/drivers/syn_rfid.c \
	$(SYN_DIR)/src/syntropic/sensor/syn_biometric.c \
	$(SYN_DIR)/src/syntropic/audio/syn_adpcm.c \
	$(SYN_DIR)/src/syntropic/audio/syn_audio.c \
	$(SYN_DIR)/src/syntropic/audio/syn_audio_mixer.c \
	$(SYN_DIR)/src/syntropic/audio/syn_sbc.c \
	$(SYN_DIR)/src/syntropic/audio/syn_wav.c \
	$(SYN_DIR)/src/syntropic/ble/syn_ble_att.c \
	$(SYN_DIR)/src/syntropic/ble/syn_ble_gap.c \
	$(SYN_DIR)/src/syntropic/ble/syn_ble_gatt.c \
	$(SYN_DIR)/src/syntropic/ble/syn_ble_hci.c \
	$(SYN_DIR)/src/syntropic/ble/syn_ble_l2cap.c \
	$(SYN_DIR)/src/syntropic/drivers/syn_usb_midi.c \
	$(SYN_DIR)/src/syntropic/drivers/syn_usb_msc.c \
	$(SYN_DIR)/src/syntropic/dsp/syn_goertzel.c \
	$(SYN_DIR)/src/syntropic/dsp/syn_mfcc.c \
	$(SYN_DIR)/src/syntropic/motor/syn_foc_encoder.c \
	$(SYN_DIR)/src/syntropic/net/syn_ntp_server.c \
	$(SYN_DIR)/src/syntropic/proto/syn_ccp.c \
	$(SYN_DIR)/src/syntropic/proto/syn_devicenet.c \
	$(SYN_DIR)/src/syntropic/proto/syn_kwp2000.c \
	$(SYN_DIR)/src/syntropic/proto/syn_ocpp.c \
	$(SYN_DIR)/src/syntropic/proto/syn_ymodem.c \
	$(SYN_DIR)/src/syntropic/sensor/syn_sensor_fusion.c \
	$(SYN_DIR)/src/syntropic/util/syn_lz4.c \
	$(SYN_DIR)/src/syntropic/util/syn_protobuf.c

SYN_STUB_SRCS := \
	$(SYN_DIR)/src/syntropic/port_stubs/syn_port_stubs.c

SYN_INC := $(SYN_DIR)/src

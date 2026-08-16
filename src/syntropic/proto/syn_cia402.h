/**
 * @file syn_cia402.h
 * @brief CANopen CiA 402 Device Profile for Drives and Motion Control.
 *
 * Implements a zero-heap, deterministic CiA 402 Drive Finite State Machine
 * (Controlword 0x6040 / Statusword 0x6041), Motion Modes of Operation (PP, PV,
 * PT, HM, CSP, CSV, CST), trapezoidal trajectory generation, following error
 * supervision, and standard CANopen Object Dictionary binding.
 */

#ifndef SYN_CIA402_H
#define SYN_CIA402_H

#if __has_include("syn_config.h")
#include "syn_config.h"
#endif

#if !defined(SYN_USE_CIA402) || SYN_USE_CIA402

#include "../common/syn_defs.h"
#include "syn_canopen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── CiA 402 Device Control States (FSM) ─────────────────────────────────── */

/**
 * @brief CiA 402 Finite State Machine state enumeration.
 */
typedef enum {
    SYN_CIA402_STATE_NOT_READY_TO_SWITCH_ON = 0, /**< Initializing / Self-test */
    SYN_CIA402_STATE_NOT_READY = 0,              /**< Alias for NOT_READY_TO_SWITCH_ON */
    SYN_CIA402_STATE_SWITCH_ON_DISABLED = 1,     /**< Main power disabled, drive safe */
    SYN_CIA402_STATE_READY_TO_SWITCH_ON = 2,     /**< Ready to power on main stage */
    SYN_CIA402_STATE_SWITCHED_ON = 3,            /**< Power stage energized, motor unexcited */
    SYN_CIA402_STATE_OPERATION_ENABLED = 4,      /**< Closed-loop motion control active */
    SYN_CIA402_STATE_QUICK_STOP_ACTIVE = 5,      /**< Fast deceleration ramp in progress */
    SYN_CIA402_STATE_FAULT_REACTION_ACTIVE = 6,  /**< Decelerating / safe shutdown on fault */
    SYN_CIA402_STATE_FAULT = 7,                  /**< Power stage tripped on fault */
} SYN_CIA402_State;

/** Alias for SYN_CIA402_State for backwards-compatibility. */
typedef SYN_CIA402_State SYN_CiA402State;

/* ── CiA 402 Modes of Operation (0x6060 / 0x6061) ────────────────────────── */

/**
 * @brief CiA 402 operation modes.
 */
typedef enum {
    SYN_CIA402_MODE_NO_MODE = 0,              /**< No active mode selected */
    SYN_CIA402_MODE_PROFILE_POSITION = 1,     /**< Profile Position Mode (PP) */
    SYN_CIA402_MODE_PP = 1,                   /**< Profile Position Mode alias */
    SYN_CIA402_MODE_PROFILE_VELOCITY = 3,     /**< Profile Velocity Mode (PV) */
    SYN_CIA402_MODE_PV = 3,                   /**< Profile Velocity Mode alias */
    SYN_CIA402_MODE_PROFILE_TORQUE = 4,       /**< Profile Torque Mode (PT) */
    SYN_CIA402_MODE_PT = 4,                   /**< Profile Torque Mode alias */
    SYN_CIA402_MODE_HOMING = 6,               /**< Homing Mode (HM) */
    SYN_CIA402_MODE_HM = 6,                   /**< Homing Mode alias */
    SYN_CIA402_MODE_CYCLIC_SYNC_POSITION = 8, /**< Cyclic Synchronous Position Mode (CSP) */
    SYN_CIA402_MODE_CSP = 8,                  /**< Cyclic Synchronous Position alias */
    SYN_CIA402_MODE_CYCLIC_SYNC_VELOCITY = 9, /**< Cyclic Synchronous Velocity Mode (CSV) */
    SYN_CIA402_MODE_CSV = 9,                  /**< Cyclic Synchronous Velocity alias */
    SYN_CIA402_MODE_CYCLIC_SYNC_TORQUE = 10,  /**< Cyclic Synchronous Torque Mode (CST) */
    SYN_CIA402_MODE_CST = 10,                 /**< Cyclic Synchronous Torque alias */
} SYN_CIA402_Mode;

/* ── Controlword (0x6040) Bit Masks ──────────────────────────────────────── */

#define SYN_CIA402_CW_SWITCH_ON (1U << 0)        /**< Bit 0: Switch On */
#define SYN_CIA402_CW_ENABLE_VOLTAGE (1U << 1)   /**< Bit 1: Enable Voltage */
#define SYN_CIA402_CW_QUICK_STOP (1U << 2)       /**< Bit 2: Quick Stop (active low: 0=stop) */
#define SYN_CIA402_CW_ENABLE_OPERATION (1U << 3) /**< Bit 3: Enable Operation */
#define SYN_CIA402_CW_NEW_SETPOINT (1U << 4) /**< Bit 4: New Set-point (PP) / Start Homing (HM) */
#define SYN_CIA402_CW_CHANGE_IMMEDIATELY                                                  \
    (1U << 5)                               /**< Bit 5: Change Set-point Immediately (PP) \
                                             */
#define SYN_CIA402_CW_RELATIVE (1U << 6)    /**< Bit 6: Relative Positioning (PP: 0=Abs, 1=Rel) */
#define SYN_CIA402_CW_FAULT_RESET (1U << 7) /**< Bit 7: Fault Reset (0 -> 1 rising edge) */
#define SYN_CIA402_CW_HALT (1U << 8)        /**< Bit 8: Halt axis */
#define SYN_CIA402_CW_CHANGE_ON_SETPOINT (1U << 9) /**< Bit 9: Change on Set-point (PP) */

/* ── Statusword (0x6041) Bit Masks ───────────────────────────────────────── */

#define SYN_CIA402_SW_READY_TO_SWITCH_ON (1U << 0) /**< Bit 0: Ready to Switch On */
#define SYN_CIA402_SW_SWITCHED_ON (1U << 1)        /**< Bit 1: Switched On */
#define SYN_CIA402_SW_OPERATION_ENABLED (1U << 2)  /**< Bit 2: Operation Enabled */
#define SYN_CIA402_SW_FAULT (1U << 3)              /**< Bit 3: Fault Active */
#define SYN_CIA402_SW_VOLTAGE_ENABLED (1U << 4)    /**< Bit 4: Power Stage Voltage Enabled */
#define SYN_CIA402_SW_QUICK_STOP \
    (1U << 5) /**< Bit 5: Quick Stop Inactive (1=normal, 0=quick stop) */
#define SYN_CIA402_SW_SWITCH_ON_DISABLED (1U << 6) /**< Bit 6: Switch On Disabled */
#define SYN_CIA402_SW_WARNING (1U << 7)            /**< Bit 7: Drive Warning */
#define SYN_CIA402_SW_MANUFACTURER_8 (1U << 8)     /**< Bit 8: Manufacturer Specific */
#define SYN_CIA402_SW_REMOTE (1U << 9)             /**< Bit 9: Remote Control Active */
#define SYN_CIA402_SW_TARGET_REACHED (1U << 10)  /**< Bit 10: Target Position / Velocity Reached */
#define SYN_CIA402_SW_INTERNAL_LIMIT (1U << 11)  /**< Bit 11: Internal / Software Limit Active */
#define SYN_CIA402_SW_SETPOINT_ACK (1U << 12)    /**< Bit 12: Setpoint Acknowledge (PP) */
#define SYN_CIA402_SW_SPEED_ZERO (1U << 12)      /**< Bit 12: Speed 0 (PV) */
#define SYN_CIA402_SW_HOMING_ATTAINED (1U << 12) /**< Bit 12: Homing Attained (HM) */
#define SYN_CIA402_SW_FOLLOWING_ERROR (1U << 13) /**< Bit 13: Following Error Active (PP) */
#define SYN_CIA402_SW_HOMING_ERROR (1U << 13)    /**< Bit 13: Homing Error (HM) */

/* ── Standard CiA 402 Object Dictionary Indices ──────────────────────────── */

#define SYN_CIA402_OD_CONTROLWORD 0x6040U                /**< Controlword (U16) */
#define SYN_CIA402_OD_STATUSWORD 0x6041U                 /**< Statusword (U16) */
#define SYN_CIA402_OD_QUICK_STOP_OPTION 0x605AU          /**< Quick Stop Option Code (I16) */
#define SYN_CIA402_OD_MODES_OF_OPERATION 0x6060U         /**< Modes of Operation (I8) */
#define SYN_CIA402_OD_MODES_OF_OPERATION_DISPLAY 0x6061U /**< Modes of Operation Display (I8) */
#define SYN_CIA402_OD_POSITION_DEMAND_VALUE 0x6062U      /**< Position Demand Value (I32) */
#define SYN_CIA402_OD_POSITION_ACTUAL_VALUE 0x6064U      /**< Position Actual Value (I32) */
#define SYN_CIA402_OD_FOLLOWING_ERROR_WINDOW 0x6065U     /**< Following Error Window (U32) */
#define SYN_CIA402_OD_POSITION_WINDOW 0x6067U            /**< Position Window (U32) */
#define SYN_CIA402_OD_VELOCITY_ACTUAL_VALUE 0x606CU      /**< Velocity Actual Value (I32) */
#define SYN_CIA402_OD_TARGET_TORQUE 0x6071U              /**< Target Torque (I16, 0.1% rated) */
#define SYN_CIA402_OD_MAX_TORQUE 0x6072U                 /**< Max Torque (U16, 0.1% rated) */
#define SYN_CIA402_OD_TORQUE_ACTUAL_VALUE 0x6077U        /**< Torque Actual Value (I16) */
#define SYN_CIA402_OD_TARGET_POSITION 0x607AU            /**< Target Position (I32) */
#define SYN_CIA402_OD_SOFTWARE_POS_LIMIT_MIN 0x607DU     /**< Software Position Limit Min (I32) */
#define SYN_CIA402_OD_SOFTWARE_POS_LIMIT_MAX 0x607EU     /**< Software Position Limit Max (I32) */
#define SYN_CIA402_OD_PROFILE_VELOCITY 0x6081U           /**< Profile Velocity (U32) */
#define SYN_CIA402_OD_PROFILE_ACCELERATION 0x6083U       /**< Profile Acceleration (U32) */
#define SYN_CIA402_OD_PROFILE_DECELERATION 0x6084U       /**< Profile Deceleration (U32) */
#define SYN_CIA402_OD_QUICK_STOP_DECELERATION 0x6085U    /**< Quick Stop Deceleration (U32) */
#define SYN_CIA402_OD_TORQUE_SLOPE 0x6087U               /**< Torque Slope (U32, units/s) */
#define SYN_CIA402_OD_HOMING_METHOD 0x6098U              /**< Homing Method (I8) */
#define SYN_CIA402_OD_HOMING_SPEED_FAST 0x6099U          /**< Homing Speed Switch (U32) */
#define SYN_CIA402_OD_HOMING_SPEED_SLOW 0x609AU          /**< Homing Speed Zero (U32) */
#define SYN_CIA402_OD_HOMING_ACCELERATION 0x609BU        /**< Homing Acceleration (U32) */
#define SYN_CIA402_OD_TARGET_VELOCITY 0x60FFU            /**< Target Velocity (I32) */

/* ── Configuration & Output Types ────────────────────────────────────────── */

/**
 * @brief CiA 402 axis configuration parameters.
 */
typedef struct {
    int32_t sw_limit_min;             /**< Software position minimum limit (counts) */
    int32_t sw_limit_max;             /**< Software position maximum limit (counts) */
    bool sw_limits_enabled;           /**< Software limit check active */
    uint32_t max_profile_velocity;    /**< Max / default profile velocity (counts/s) */
    uint32_t profile_acceleration;    /**< Default profile acceleration (counts/s^2) */
    uint32_t profile_deceleration;    /**< Default profile deceleration (counts/s^2) */
    uint32_t quick_stop_deceleration; /**< Quick stop deceleration rate (counts/s^2) */
    uint32_t following_err_window;    /**< Following error threshold (0=disabled) */
    uint32_t position_window;         /**< Target reached deadband (counts) */
    uint16_t max_torque;              /**< Max torque limit (0.1% rated) */
    uint32_t torque_slope;            /**< Torque ramp rate (0.1%/s) */
} SYN_CIA402_Config;

/** Alias for SYN_CIA402_Config for backwards-compatibility. */
typedef SYN_CIA402_Config SYN_CiA402Config;

/**
 * @brief CiA 402 motion controller setpoint outputs for motor loop / FOC.
 */
typedef struct {
    bool power_enabled;   /**< True when operation enabled / bridge active */
    bool brake_released;  /**< True when holding brake should be released */
    SYN_CIA402_Mode mode; /**< Active control mode */
    int32_t position_cmd; /**< Demanded position setpoint (counts) */
    int32_t velocity_cmd; /**< Demanded velocity setpoint (counts/s) */
    int16_t torque_cmd;   /**< Demanded torque setpoint (0.1% rated) */
} SYN_CIA402_Setpoints;

/**
 * @brief CiA 402 Axis Instance.
 */
typedef struct {
    /* State & Mode */
    SYN_CIA402_State state;        /**< Current FSM state */
    int8_t mode_of_operation;      /**< Mode of operation requested (0x6060) */
    int8_t mode_of_operation_disp; /**< Mode of operation display (0x6061) */
    uint16_t controlword;          /**< Active Controlword (0x6040) */
    uint16_t statusword;           /**< Active Statusword (0x6041) */
    uint16_t prev_controlword;     /**< Previous Controlword for edge detection */
    uint16_t error_code;           /**< Last triggered error code */
    int16_t quick_stop_option;     /**< Quick stop option code (0x605A) */

    /* Live Feedback & Status Variables */
    int32_t actual_position; /**< 0x6064 Position Actual Value */
    int32_t actual_velocity; /**< 0x606C Velocity Actual Value */
    int16_t actual_torque;   /**< 0x6077 Torque Actual Value */
    bool target_reached;     /**< Target reached flag */
    bool setpoint_ack;       /**< Setpoint acknowledge handshake flag */
    float current_speed;     /**< Current internal floating speed for tests */

    /* Live Object Dictionary Variables */
    int32_t position_demand;    /**< 0x6062 Position Demand Value */
    uint32_t following_err_win; /**< 0x6065 Following Error Window */
    uint32_t position_window;   /**< 0x6067 Position Window */
    int16_t target_torque;      /**< 0x6071 Target Torque */
    uint16_t max_torque;        /**< 0x6072 Max Torque */
    int32_t target_position;    /**< 0x607A Target Position */
    int32_t sw_limit_min;       /**< 0x607D Software Limit Min */
    int32_t sw_limit_max;       /**< 0x607E Software Limit Max */
    uint32_t profile_vel;       /**< 0x6081 Profile Velocity */
    uint32_t profile_acc;       /**< 0x6083 Profile Acceleration */
    uint32_t profile_dec;       /**< 0x6084 Profile Deceleration */
    uint32_t quick_stop_dec;    /**< 0x6085 Quick Stop Deceleration */
    uint32_t torque_slope;      /**< 0x6087 Torque Slope */
    int8_t homing_method;       /**< 0x6098 Homing Method */
    uint32_t homing_speed_fast; /**< 0x6099:1 Homing Speed Fast */
    uint32_t homing_speed_slow; /**< 0x6099:2 Homing Speed Slow */
    uint32_t homing_accel;      /**< 0x609B Homing Acceleration */
    int32_t target_velocity;    /**< 0x60FF Target Velocity */

    /* Internal Motion Profiler State */
    int64_t current_pos_fxp;    /**< Internal high-precision position (Q32.32) */
    int64_t current_vel_fxp;    /**< Internal high-precision velocity (Q32.32) */
    int32_t current_torque_fxp; /**< Internal high-precision torque (Q16.16) */
    bool sw_limits_enabled;     /**< Software limit check active */
    bool homing_in_progress;    /**< Active homing sequence */
    bool homing_attained;       /**< Homing completed successfully */
    bool homing_error;          /**< Homing fault occurred */
} SYN_CIA402_Axis;

/** Alias for SYN_CIA402_Axis for backwards-compatibility. */
typedef SYN_CIA402_Axis SYN_CiA402Drive;

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * @brief Initialize CiA 402 Axis controller.
 *
 * @param axis  Axis instance.
 * @param cfg   Initial configuration parameters.
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL parameters.
 */
SYN_Status syn_cia402_init(SYN_CIA402_Axis *axis, const SYN_CIA402_Config *cfg);

/**
 * @brief Set Controlword (Index 0x6040) from master / RPDO / SDO.
 *
 * @param axis  Axis instance.
 * @param cw    Controlword value.
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL.
 */
SYN_Status syn_cia402_set_controlword(SYN_CIA402_Axis *axis, uint16_t cw);

/**
 * @brief Read current Controlword (Index 0x6040).
 *
 * @param axis  Axis instance.
 * @return Controlword value (0 on NULL).
 */
uint16_t syn_cia402_get_controlword(const SYN_CIA402_Axis *axis);

/**
 * @brief Read current Statusword (Index 0x6041).
 *
 * @param axis  Axis instance.
 * @return Statusword value (0 on NULL).
 */
uint16_t syn_cia402_get_statusword(const SYN_CIA402_Axis *axis);

/**
 * @brief Get current FSM state.
 *
 * @param axis  Axis instance.
 * @return Current FSM state.
 */
SYN_CIA402_State syn_cia402_get_state(const SYN_CIA402_Axis *axis);

/**
 * @brief Request Mode of Operation (Index 0x6060).
 *
 * @param axis  Axis instance.
 * @param mode  Requested mode (SYN_CIA402_MODE_*).
 * @return SYN_OK on success, SYN_INVALID_PARAM on invalid mode.
 */
SYN_Status syn_cia402_set_mode(SYN_CIA402_Axis *axis, int8_t mode);

/**
 * @brief Get requested Mode of Operation (Index 0x6060).
 *
 * @param axis  Axis instance.
 * @return Mode of operation.
 */
int8_t syn_cia402_get_mode(const SYN_CIA402_Axis *axis);

/**
 * @brief Get active Mode of Operation Display (Index 0x6061).
 *
 * @param axis  Axis instance.
 * @return Mode of operation display.
 */
int8_t syn_cia402_get_mode_display(const SYN_CIA402_Axis *axis);

/**
 * @brief Set target position (Index 0x607A).
 *
 * @param axis  Axis instance.
 * @param pos   Target position in counts.
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL.
 */
SYN_Status syn_cia402_set_target_position(SYN_CIA402_Axis *axis, int32_t pos);

/**
 * @brief Set target velocity (Index 0x60FF).
 *
 * @param axis  Axis instance.
 * @param vel   Target velocity in counts/s.
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL.
 */
SYN_Status syn_cia402_set_target_velocity(SYN_CIA402_Axis *axis, int32_t vel);

/**
 * @brief Set target torque (Index 0x6071).
 *
 * @param axis    Axis instance.
 * @param torque  Target torque in 0.1% rated torque.
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL.
 */
SYN_Status syn_cia402_set_target_torque(SYN_CIA402_Axis *axis, int16_t torque);

/**
 * @brief Feed measured actual values (encoder position, velocity, torque) to axis.
 *
 * @param axis           Axis instance.
 * @param pos_actual     Actual position (counts).
 * @param vel_actual     Actual velocity (counts/s).
 * @param torque_actual  Actual torque (0.1% rated).
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL.
 */
SYN_Status syn_cia402_feed_feedback(SYN_CIA402_Axis *axis, int32_t pos_actual, int32_t vel_actual,
                                    int16_t torque_actual);

/**
 * @brief Feed measured actual values alias for compatibility.
 *
 * @param axis           Axis instance.
 * @param pos_actual     Actual position (counts).
 * @param vel_actual     Actual velocity (counts/s).
 * @param torque_actual  Actual torque (0.1% rated).
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL.
 */
SYN_Status syn_cia402_report_actuals(SYN_CIA402_Axis *axis, int32_t pos_actual, int32_t vel_actual,
                                     int16_t torque_actual);

/**
 * @brief Periodic motion update in milliseconds.
 *
 * @param axis   Axis instance.
 * @param dt_ms  Time delta in milliseconds.
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL.
 */
SYN_Status syn_cia402_update(SYN_CIA402_Axis *axis, uint32_t dt_ms);

/**
 * @brief Execute periodic CiA 402 control loop step in microseconds.
 *
 * Evaluates state machine transitions, computes motion ramps, performs following
 * error supervision, updates Statusword, and outputs motor setpoints.
 *
 * @param axis          Axis instance.
 * @param dt_us         Time elapsed since last step in microseconds.
 * @param out_setpoints Output setpoint commands (may be NULL if not needed).
 * @return SYN_OK on success, error code on failure.
 */
SYN_Status syn_cia402_step(SYN_CIA402_Axis *axis, uint32_t dt_us,
                           SYN_CIA402_Setpoints *out_setpoints);

/**
 * @brief Trigger a drive fault and enter FAULT_REACTION_ACTIVE / FAULT state.
 *
 * @param axis        Axis instance.
 * @param error_code  Fault error code (e.g. overcurrent, limit tripped, tracking error).
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL.
 */
SYN_Status syn_cia402_trigger_fault(SYN_CIA402_Axis *axis, uint16_t error_code);

/**
 * @brief Populate a CANopen Object Dictionary table with standard CiA 402 objects.
 *
 * Binds 0x6040 (Controlword), 0x6041 (Statusword), 0x6060..0x60FF objects directly
 * to the live fields of the given axis instance.
 *
 * @param od_table     Destination array of Object Dictionary entries.
 * @param max_entries  Size of od_table.
 * @param axis         Axis instance to bind.
 * @return Number of entries populated (0 on error or insufficient capacity).
 */
size_t syn_cia402_populate_od(SYN_CANOpenODEntry *od_table, size_t max_entries,
                              SYN_CIA402_Axis *axis);

#ifdef __cplusplus
}
#endif

#endif /* SYN_USE_CIA402 */

#endif /* SYN_CIA402_H */

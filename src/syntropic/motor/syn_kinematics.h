/**
 * @file syn_kinematics.h
 * @brief Multi-Axis Robot Forward & Inverse Kinematics Engine (Q16.16 fixed-point).
 * @ingroup syn_motor
 *
 * Provides a zero-heap robotics kinematics engine for embedded microcontrollers:
 * - Standard & Modified Denavit-Hartenberg (DH) parameter chain transforms.
 * - Multi-joint forward kinematics with position vector and orientation extraction.
 * - Closed-form inverse kinematics solvers for:
 *   - 3-DOF Planar Articulated Arm.
 *   - 4-DOF SCARA Robot (X, Y, Z, Yaw) with elbow-left / elbow-right configurations.
 *   - 6-DOF Articulated Robot Arm with spherical wrist (Pieper's decoupling).
 *   - 3-Axis Delta Parallel Robot.
 * - Geometric Jacobian computation (6 x N) for differential velocity kinematics.
 */

#ifndef SYN_KINEMATICS_H
#define SYN_KINEMATICS_H

#include "../common/syn_defs.h"
#include "../util/syn_matrix.h"
#include "../util/syn_qmath.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(SYN_USE_KINEMATICS) || SYN_USE_KINEMATICS

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants & Limits ─────────────────────────────────────────────────── */

#define SYN_KINEMATICS_MAX_JOINTS 8U /**< Maximum supported joints in a kinematic chain */

/* ── Denavit-Hartenberg Types ────────────────────────────────────────────── */

/**
 * @brief Denavit-Hartenberg parameter convention.
 */
typedef enum {
    SYN_DH_CONVENTION_STANDARD = 0, /**< Standard DH convention (Craig / Paul) */
    SYN_DH_CONVENTION_MODIFIED = 1  /**< Modified DH convention (Khalil / Kleinfinger) */
} SYN_DH_Convention;

/**
 * @brief Single joint Denavit-Hartenberg parameter specification.
 */
typedef struct {
    q16_t a;           /**< Link length along X axis */
    q16_t alpha;       /**< Link twist angle around X axis (radians) */
    q16_t d;           /**< Link offset along Z axis (prismatic joint variable if is_prismatic) */
    q16_t theta;       /**< Joint angle around Z axis (revolute joint variable if !is_prismatic) */
    bool is_prismatic; /**< True if prismatic (linear) joint, false if revolute */
    q16_t min_limit;   /**< Minimum joint position limit */
    q16_t max_limit;   /**< Maximum joint position limit */
} SYN_DH_Param;

/* ── Spatial Pose Types ──────────────────────────────────────────────────── */

/**
 * @brief 3D Cartesian Position (X, Y, Z) in Q16.16.
 */
typedef struct {
    q16_t x; /**< X coordinate */
    q16_t y; /**< Y coordinate */
    q16_t z; /**< Z coordinate */
} SYN_Position3D;

/**
 * @brief 3D Orientation (Roll, Pitch, Yaw Euler angles) in Q16.16 radians.
 */
typedef struct {
    q16_t roll;  /**< Roll angle around X axis */
    q16_t pitch; /**< Pitch angle around Y axis */
    q16_t yaw;   /**< Yaw angle around Z axis */
} SYN_Orientation3D;

/**
 * @brief Complete 6-DOF Spatial Pose (Position + Orientation).
 */
typedef struct {
    SYN_Position3D position;       /**< Cartesian position */
    SYN_Orientation3D orientation; /**< Euler orientation */
} SYN_Pose6D;

/* ── Mechanism Configuration Descriptors ─────────────────────────────────── */

/**
 * @brief Configuration parameters for 3-DOF Planar Arm.
 */
typedef struct {
    q16_t l1; /**< Link 1 length */
    q16_t l2; /**< Link 2 length */
    q16_t l3; /**< Link 3 length */
} SYN_Kinematics_Planar3Config;

/**
 * @brief Elbow configuration for 2D/3D arm inverse kinematics.
 */
typedef enum {
    SYN_ARM_ELBOW_UP = 0,  /**< Elbow up / Left-handed configuration */
    SYN_ARM_ELBOW_DOWN = 1 /**< Elbow down / Right-handed configuration */
} SYN_ArmElbow;

/**
 * @brief Configuration parameters for 4-DOF SCARA Robot.
 */
typedef struct {
    q16_t l1;     /**< Link 1 length (base to joint 2) */
    q16_t l2;     /**< Link 2 length (joint 2 to quill) */
    q16_t d_max;  /**< Maximum vertical stroke (Z axis) */
    q16_t z_home; /**< Z-axis home offset */
} SYN_Kinematics_SCARAConfig;

/**
 * @brief Configuration parameters for 6-DOF Articulated Arm with Spherical Wrist.
 */
typedef struct {
    q16_t d1; /**< Base height (Z offset from base to shoulder) */
    q16_t a1; /**< Shoulder offset along X axis */
    q16_t a2; /**< Upper arm length (shoulder to elbow) */
    q16_t d4; /**< Forearm length (elbow to wrist center) */
    q16_t d6; /**< Tool flange offset (wrist center to tool tip) */
} SYN_Kinematics_6DOFConfig;

/**
 * @brief Configuration parameters for 3-Axis Delta Parallel Robot.
 */
typedef struct {
    q16_t base_radius;     /**< Base equilateral triangle / platform radius (sb) */
    q16_t effector_radius; /**< End-effector platform radius (sp) */
    q16_t upper_arm_len;   /**< Upper arm / bicep length (L) */
    q16_t lower_arm_len;   /**< Lower parallel arm / parallelogram rod length (l) */
} SYN_Kinematics_DeltaConfig;

/* ── General Denavit-Hartenberg & Chain API ──────────────────────────────── */

/**
 * @brief Compute single-link 4x4 Homogeneous Transformation Matrix from DH parameters.
 * @param param DH parameters for link.
 * @param joint_val Joint variable (angle in rad for revolute, displacement for prismatic).
 * @param conv DH convention (standard or modified).
 * @param out_t44 Output 4x4 matrix (must be initialized with 4 rows, 4 cols).
 * @return SYN_OK on success, SYN_INVALID_PARAM on NULL or dimension error.
 */
SYN_Status syn_dh_matrix(const SYN_DH_Param *param, q16_t joint_val, SYN_DH_Convention conv,
                         SYN_Matrix *out_t44);

/**
 * @brief Compute Forward Kinematics for a general serial DH chain.
 * @param chain Array of DH joint parameters.
 * @param num_joints Number of joints in chain (<= SYN_KINEMATICS_MAX_JOINTS).
 * @param joint_vals Array of joint values.
 * @param conv DH convention.
 * @param out_t44 Optional output 4x4 end-effector transform matrix (can be NULL).
 * @param out_pose Optional output 6-DOF pose (can be NULL).
 * @return SYN_OK on success, SYN_INVALID_PARAM on invalid inputs.
 */
SYN_Status syn_kinematics_forward(const SYN_DH_Param *chain, size_t num_joints,
                                  const q16_t *joint_vals, SYN_DH_Convention conv,
                                  SYN_Matrix *out_t44, SYN_Pose6D *out_pose);

/**
 * @brief Calculate Geometric 6xN Jacobian Matrix for a serial link chain.
 * @param chain Array of DH joint parameters.
 * @param num_joints Number of joints in chain.
 * @param joint_vals Current joint positions.
 * @param conv DH convention.
 * @param out_j6xn Output 6xN matrix (rows=6, cols=num_joints).
 * @return SYN_OK on success, SYN_INVALID_PARAM on invalid inputs.
 */
SYN_Status syn_kinematics_jacobian(const SYN_DH_Param *chain, size_t num_joints,
                                   const q16_t *joint_vals, SYN_DH_Convention conv,
                                   SYN_Matrix *out_j6xn);

/* ── 3-DOF Planar Arm API ────────────────────────────────────────────────── */

/**
 * @brief Forward kinematics for 3-DOF planar arm.
 * @param cfg Planar arm configuration.
 * @param q1 Joint 1 angle (rad).
 * @param q2 Joint 2 angle (rad).
 * @param q3 Joint 3 angle (rad).
 * @param out_x Pointer to receive X position.
 * @param out_y Pointer to receive Y position.
 * @param out_phi Pointer to receive tool orientation angle (rad).
 * @return SYN_OK on success.
 */
SYN_Status syn_kinematics_planar3_fk(const SYN_Kinematics_Planar3Config *cfg, q16_t q1, q16_t q2,
                                     q16_t q3, q16_t *out_x, q16_t *out_y, q16_t *out_phi);

/**
 * @brief Closed-form inverse kinematics for 3-DOF planar arm.
 * @param cfg Planar arm configuration.
 * @param x Target X position.
 * @param y Target Y position.
 * @param phi Target end-effector orientation angle (rad).
 * @param elbow Elbow configuration (up or down).
 * @param out_q1 Output joint 1 angle.
 * @param out_q2 Output joint 2 angle.
 * @param out_q3 Output joint 3 angle.
 * @return SYN_OK on success, SYN_ERROR if target unreachable.
 */
SYN_Status syn_kinematics_planar3_ik(const SYN_Kinematics_Planar3Config *cfg, q16_t x, q16_t y,
                                     q16_t phi, SYN_ArmElbow elbow, q16_t *out_q1, q16_t *out_q2,
                                     q16_t *out_q3);

/* ── 4-DOF SCARA API ─────────────────────────────────────────────────────── */

/**
 * @brief Forward kinematics for 4-DOF SCARA robot.
 * @param cfg SCARA configuration.
 * @param q1 Joint 1 shoulder angle (rad).
 * @param q2 Joint 2 elbow angle (rad).
 * @param d3 Joint 3 prismatic vertical displacement (distance down).
 * @param q4 Joint 4 wrist roll angle (rad).
 * @param out_pose Output 4-DOF pose (X, Y, Z, Yaw).
 * @return SYN_OK on success.
 */
SYN_Status syn_kinematics_scara_fk(const SYN_Kinematics_SCARAConfig *cfg, q16_t q1, q16_t q2,
                                   q16_t d3, q16_t q4, SYN_Pose6D *out_pose);

/**
 * @brief Closed-form inverse kinematics for 4-DOF SCARA robot.
 * @param cfg SCARA configuration.
 * @param target Target 6-DOF pose (X, Y, Z, Yaw).
 * @param elbow Elbow configuration (up/left or down/right).
 * @param out_q1 Output joint 1 angle.
 * @param out_q2 Output joint 2 angle.
 * @param out_d3 Output joint 3 vertical position.
 * @param out_q4 Output joint 4 wrist angle.
 * @return SYN_OK on success, SYN_ERROR if target unreachable.
 */
SYN_Status syn_kinematics_scara_ik(const SYN_Kinematics_SCARAConfig *cfg, const SYN_Pose6D *target,
                                   SYN_ArmElbow elbow, q16_t *out_q1, q16_t *out_q2, q16_t *out_d3,
                                   q16_t *out_q4);

/* ── 6-DOF Articulated Arm (Spherical Wrist) API ─────────────────────────── */

/**
 * @brief Forward kinematics for 6-DOF articulated arm.
 * @param cfg 6-DOF arm configuration.
 * @param q Array of 6 joint angles in radians.
 * @param out_pose Output 6-DOF Cartesian pose.
 * @return SYN_OK on success.
 */
SYN_Status syn_kinematics_6dof_fk(const SYN_Kinematics_6DOFConfig *cfg, const q16_t q[6],
                                  SYN_Pose6D *out_pose);

/**
 * @brief Closed-form inverse kinematics for 6-DOF arm with spherical wrist (Pieper solution).
 * @param cfg 6-DOF arm configuration.
 * @param target Target 6-DOF pose.
 * @param elbow Shoulder/elbow solution choice.
 * @param wrist_flip Invert wrist pitch/yaw solution if true.
 * @param out_q Output array of 6 joint angles in radians.
 * @return SYN_OK on success, SYN_ERROR if target position or orientation unreachable.
 */
SYN_Status syn_kinematics_6dof_ik(const SYN_Kinematics_6DOFConfig *cfg, const SYN_Pose6D *target,
                                  SYN_ArmElbow elbow, bool wrist_flip, q16_t out_q[6]);

/* ── 3-Axis Delta Parallel Robot API ─────────────────────────────────────── */

/**
 * @brief Forward kinematics for 3-axis Delta parallel robot.
 * @param cfg Delta robot geometry configuration.
 * @param theta1 Actuator 1 angle (rad).
 * @param theta2 Actuator 2 angle (rad).
 * @param theta3 Actuator 3 angle (rad).
 * @param out_pos Output 3D Cartesian position of traveling plate center.
 * @return SYN_OK on success, SYN_ERROR if no real geometric intersection exists.
 */
SYN_Status syn_kinematics_delta_fk(const SYN_Kinematics_DeltaConfig *cfg, q16_t theta1,
                                   q16_t theta2, q16_t theta3, SYN_Position3D *out_pos);

/**
 * @brief Inverse kinematics for 3-axis Delta parallel robot.
 * @param cfg Delta robot geometry configuration.
 * @param target Target 3D Cartesian position (X, Y, Z).
 * @param out_theta1 Output actuator 1 angle (rad).
 * @param out_theta2 Output actuator 2 angle (rad).
 * @param out_theta3 Output actuator 3 angle (rad).
 * @return SYN_OK on success, SYN_ERROR if position is outside workspace.
 */
SYN_Status syn_kinematics_delta_ik(const SYN_Kinematics_DeltaConfig *cfg,
                                   const SYN_Position3D *target, q16_t *out_theta1,
                                   q16_t *out_theta2, q16_t *out_theta3);

#ifdef __cplusplus
}
#endif

#endif /* !defined(SYN_USE_KINEMATICS) || SYN_USE_KINEMATICS */

#endif /* SYN_KINEMATICS_H */

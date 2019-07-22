//
// Created by liuzikai on 2019-01-30.
//

#ifndef META_INFANTRY_VEHICLE_ENGINEER_H
#define META_INFANTRY_VEHICLE_ENGINEER_H

/// Mechanism Parameters
#define CHASSIS_WHEEL_BASE  485.0f  // distance between front axle and the back axle, mm
#define CHASSIS_WHEEL_TREAD 460.0f  // distance between left and right wheels, mm
#define CHASSIS_WHEEL_CIRCUMFERENCE 479.1f  // mm

// TODO: embed as constant
#define ENGINEER_CHASSIS_VELOCITY_MAX 1000.0f
#define ENGINEER_CHASSIS_W_MAX 150.0f
#define ENGINEER_AIDED_MOTOR_VELOCITY 600.0f
#define ANGLE_HEIGHT_RATIO 358.2f   // [degree/cm]
#define STAGE_HEIGHT 24.0f          // cm // TODO need to measure

#define ROBOTIC_ARM_STRETCH_OUT_ANGLE 120.0f
#define ROBOTIC_ARM_PULL_BACK_ANGLE 30.0f
#define ROBOTIC_ARM_ROTATE_VELOCITY 50.0f
#define ROBOTIC_ARM_TRIGGER_VELOCITY 5.0f

/// Chassis PID Parameters
#define CHASSIS_PID_V2I_KP 33.0f
#define CHASSIS_PID_V2I_KI 0.49f
#define CHASSIS_PID_V2I_KD 2.4f
#define CHASSIS_PID_V2I_I_LIMIT 4000.0f
#define CHASSIS_PID_V2I_OUT_LIMIT 5000.0f
#define CHASSIS_PID_V2I_PARAMS \
    {CHASSIS_PID_V2I_KP, CHASSIS_PID_V2I_KI, CHASSIS_PID_V2I_KD, \
    CHASSIS_PID_V2I_I_LIMIT, CHASSIS_PID_V2I_OUT_LIMIT}

/// Elevator and Aided Motors PID Parameters
#define ELEVATOR_PID_A2V_KP 3.0f
#define ELEVATOR_PID_A2V_KI 0.0f
#define ELEVATOR_PID_A2V_KD 0.0f
#define ELEVATOR_PID_A2V_V_LIMIT 3500.0f
#define ELEVATOR_PID_A2V_OUT_LIMIT 2000.0f
#define ELEVATOR_PID_A2V_PARAMS \
    {ELEVATOR_PID_A2V_KP, ELEVATOR_PID_A2V_KI, ELEVATOR_PID_A2V_KD, \
    ELEVATOR_PID_A2V_V_LIMIT, ELEVATOR_PID_A2V_OUT_LIMIT}

#define ELEVATOR_PID_V2I_KP 1.5f
#define ELEVATOR_PID_V2I_KI 0.022f
#define ELEVATOR_PID_V2I_KD 0.0f
#define ELEVATOR_PID_V2I_I_LIMIT 4000.0f
#define ELEVATOR_PID_V2I_OUT_LIMIT 5000.0f
#define ELEVATOR_PID_V2I_PARAMS \
    {ELEVATOR_PID_V2I_KP, ELEVATOR_PID_V2I_KI, ELEVATOR_PID_V2I_KD, \
    ELEVATOR_PID_V2I_I_LIMIT, ELEVATOR_PID_V2I_OUT_LIMIT}

#define AIDED_MOTOR_PID_V2I_KP 15.0f
#define AIDED_MOTOR_PID_V2I_KI 0.22f
#define AIDED_MOTOR_PID_V2I_KD 0.0f
#define AIDED_MOTOR_PID_V2I_I_LIMIT 4000.0f
#define AIDED_MOTOR_PID_V2I_OUT_LIMIT 5000.0f
#define AIDED_MOTOR_PID_V2I_PARAMS \
    {AIDED_MOTOR_PID_V2I_KP, AIDED_MOTOR_PID_V2I_KI, AIDED_MOTOR_PID_V2I_KD, \
    AIDED_MOTOR_PID_V2I_I_LIMIT, AIDED_MOTOR_PID_V2I_OUT_LIMIT}

/// Robotic Arm PID Parameters
#define ROBOTIC_ARM_PID_V2I_KP 0.0f
#define ROBOTIC_ARM_PID_V2I_KI 0.0f
#define ROBOTIC_ARM_PID_V2I_KD 0.0f
#define ROBOTIC_ARM_PID_V2I_I_LIMIT 0.0f
#define ROBOTIC_ARM_PID_V2I_OUT_LIMIT 0.0f
#define ROBOTIC_ARM_PID_V2I_PARAMS \
    {ROBOTIC_ARM_PID_V2I_KP, ROBOTIC_ARM_PID_V2I_KI, ROBOTIC_ARM_PID_V2I_KD, \
    ROBOTIC_ARM_PID_V2I_I_LIMIT, ROBOTIC_ARM_PID_V2I_OUT_LIMIT}

/// Thread Priority List
#define THREAD_CAN1_PRIO                    (HIGHPRIO - 1)
#define THREAD_CAN2_PRIO                    (HIGHPRIO - 2)
#define THREAD_CHASSIS_SKD_PRIO             (NORMALPRIO + 3)
#define THREAD_ELEVATOR_SKD_PRIO            (NORMALPRIO + 2)
#define THREAD_ROBOTIC_ARM_SKD_PRIO         (NORMALPRIO + 1)
#define THREAD_USER_PRIO                    (NORMALPRIO)
#define THREAD_USER_ACTION_PRIO             (NORMALPRIO - 1)
#define THREAD_ELEVATOR_LG_PRIO             (NORMALPRIO - 2)
#define THREAD_INSPECTOR_PRIO               (NORMALPRIO - 10)
#define THREAD_INSPECTOR_REFEREE_PRIO       (NORMALPRIO - 11)
#define THREAD_USER_CLIENT_DATA_SEND_PRIO   (LOWPRIO + 6)
#define THREAD_SHELL_PRIO                   (LOWPRIO + 5)
#define THREAD_BUZZER_PRIO                  (LOWPRIO)

/// Dev Board LED Usage List
#define DEV_BOARD_LED_SYSTEM_INIT 1
#define DEV_BOARD_LED_CAN         2
#define DEV_BOARD_LED_DMS         3
#define DEV_BOARD_LED_REMOTE      4
#define DEV_BOARD_LED_ELEVATOR    5
#define DEV_BOARD_LED_CHASSIS     6
#define DEV_BOARD_LED_REFEREE     7  // used in infantry ShootLG BulletCounterThread
#define DEV_BOARD_LED_SD_CARD     8

/// User Client Usage List
#define USER_CLIENT_SPEED_LEVEL_3_LIGHT             3
#define USER_CLIENT_SPEED_LEVEL_2_LIGHT             4
#define USER_CLIENT_SPEED_LEVEL_1_LIGHT             5

    
#endif //META_INFANTRY_VEHICLE_ENGINEER_H

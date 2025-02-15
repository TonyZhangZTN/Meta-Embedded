//
// Created by liuzikai on 2019-01-27.
//

/// Headers
#include "ch.hpp"
#include "hal.h"

#include "led.h"
#include "buzzer_scheduler.h"
#include "common_macro.h"

#include "shell.h"
#include "ahrs.h"
#include "remote_interpreter.h"
#include "sd_card_interface.h"

#include "can_interface.h"
#include "capacitor_interface.h"
#include "lidar_interface.h"

#include "referee_UI_update_scheduler.h"
#include "referee_UI_logic.h"

#include "can_motor_interface.h"
#include "can_motor_controller.h"

#include "gimbal_scheduler.h"
#include "shoot_scheduler.h"
#include "gimbal_logic.h"
#include "shoot_logic.h"
#include "mecanum_chassis_scheduler.h"
#include "chassis_logic.h"

//#include "vision_interface.h"
//#include "vision_scheduler.h"

#include "thread_priorities.h"
#ifndef PARAM_ADJUST
#include "user_hero.h"
#else
#include PARAM_ADJUST_INCLUDE
#endif
#include "shellconf.h"
/// Vehicle Specific Configurations

#include "vehicle_hero.h"
//ZTN DEBUG
#include <printf.h>
/// Board Guard
#if defined(BOARD_RM_2018_A)
#else
#error "Hero supports only RM Board 2018 A currently"
#endif

/// Instances
CANInterface can1(&CAND1);
CANInterface can2(&CAND2);
AHRSOnBoard ahrs;

/// Local Constants
static const Matrix33 ON_BOARD_AHRS_MATRIX_ = ON_BOARD_AHRS_MATRIX;
static const Matrix33 GIMBAL_ANGLE_INSTALLATION_MATRIX_ = GIMBAL_ANGLE_INSTALLATION_MATRIX;
static const Matrix33 GIMBAL_GYRO_INSTALLATION_MATRIX_ = GIMBAL_GYRO_INSTALLATION_MATRIX;

int main() {

    /*** --------------------------- Period 0. Fundamental Setup --------------------------- ***/

    halInit();
    chibios_rt::System::init();

    // Enable power of bullet loader motor
    palSetPadMode(GPIOH, GPIOH_POWER1_CTRL, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPad(GPIOH, GPIOH_POWER1_CTRL);

    // Enable power of ultraviolet lights
    palSetPadMode(GPIOH, GPIOH_POWER2_CTRL, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPad(GPIOH, GPIOH_POWER2_CTRL);

    /*** ---------------------- Period 1. Modules Setup and Self-Check ---------------------- ***/

    /// Preparation of Period 1, init inspector
    InspectorH::init(&can1, &can2, &ahrs);
    LED::all_off();

    /// Setup Shell
    Shell::start(THREAD_SHELL_PRIO);
    chThdSleepMilliseconds(50);  // wait for logo to print :)

    BuzzerSKD::init(THREAD_BUZZER_SKD_PRIO);
    /// Setup SDCard
    if (SDCard::init()) {
        SDCard::read_all();
        LED::led_on(DEV_BOARD_LED_SD_CARD);  // LED 8 on if SD card inserted
    }

    LED::led_on(DEV_BOARD_LED_SYSTEM_INIT);  // LED 1 on now

    /// Setup CAN1 & CAN2 BUS
    can1.start(THREAD_CAN1_RX_PRIO);
    can2.start(THREAD_CAN2_RX_PRIO);
    chThdSleepMilliseconds(5);
    InspectorH::startup_check_can();  // check no persistent CAN Error. Block for 100 ms
    LED::led_on(DEV_BOARD_LED_CAN);  // LED 2 on now

    /// Setup CapacitorIF Port
    CapacitorIF::init(&can2, THREAD_SUPERCAP_INIT_PRIO);
    LidarIF::init(&can2);

    /// Setup Referee
    Referee::init();
    RefereeUISKD::init(THREAD_REFEREE_SKD_PRIO);
    RefereeUILG::reset();

    /// Complete Period 1
    LED::green_on();  // LED Green on now

    /// Setup On-Board AHRS
    Vector3D ahrs_bias;
    if (SDCard::get_data(MPU6500_BIAS_DATA_ID, &ahrs_bias, sizeof(ahrs_bias)) == SDCard::OK) {
        ahrs.load_calibration_data(ahrs_bias);
        LOG("Use AHRS bias in SD Card");
    } else {
        ahrs.load_calibration_data(MPU6500_STORED_GYRO_BIAS);
        LOG_WARN("Use default AHRS bias");
    }
    ahrs.start(ON_BOARD_AHRS_MATRIX_, THREAD_AHRS_PRIO);
    while(!ahrs.ready()) {
        chThdSleepMilliseconds(5);
    }
    InspectorH::startup_check_mpu();  // check MPU6500 has signal. Block for 20 ms
    InspectorH::startup_check_ist();  // check IST8310 has signal. Block for 20 ms
    Shell::addCommands(ahrs.shellCommands);
    Shell::addFeedbackCallback(AHRSOnBoard::cmdFeedback, &ahrs);
    LED::led_on(DEV_BOARD_LED_AHRS);  // LED 3 on now

    /// Setup Remote
    Remote::start();
    InspectorH::startup_check_remote();  // check Remote has signal. Block for 50 ms
    LED::led_on(DEV_BOARD_LED_REMOTE);  // LED 4 on now

    /// Setup CAN
    CANMotorController::start(THREAD_MOTOR_SKD_PRIO, THREAD_FEEDBACK_SKD_PRIO, &can1, &can2);

#ifndef DEBUG_NO_GIMBAL
    chThdSleepMilliseconds(2000);  // wait for C610 to be online and friction wheel to reset
    /// Setup GimbalIF (for Gimbal and Shoot)
    //InspectorH::startup_check_gimbal_feedback(); // check gimbal motors has continuous feedback. Block for 20 ms
    LED::led_on(DEV_BOARD_LED_GIMBAL);  // LED 5 on now
#endif

#ifndef DEBUG_NO_CHASSIS
    /// Setup ChassisIF
    chThdSleepMilliseconds(10);
    InspectorH::startup_check_chassis_feedback();  // check chassis motors has continuous feedback. Block for 20 ms
    LED::led_on(DEV_BOARD_LED_CHASSIS);  // LED 6 on now
#endif

    /// Setup Red Spot Laser and Lidar
    palSetPad(GPIOG, GPIOG_RED_SPOT_LASER);  // enable the red spot laser

    /*** ------------ Period 2. Calibration and Start Logic Control Thread ----------- ***/

    /// Echo Gimbal Raws and Converted Angles
    LOG("Gimbal Yaw: %u, %f, Pitch: %u, %f, Sub Pitch: %u, %f",
        CANMotorIF::motor_feedback[CANMotorCFG::YAW].last_rotor_angle_raw,
        CANMotorIF::motor_feedback[CANMotorCFG::YAW].accumulate_angle(),
        CANMotorIF::motor_feedback[CANMotorCFG::PITCH].last_rotor_angle_raw,
        CANMotorIF::motor_feedback[CANMotorCFG::PITCH].accumulate_angle(),
        CANMotorIF::motor_feedback[CANMotorCFG::SUB_PITCH].last_rotor_angle_raw,
        CANMotorIF::motor_feedback[CANMotorCFG::SUB_PITCH].accumulate_angle());

    /// Start SKDs
#if ENABLE_AHRS
#ifndef DEBUG_NO_GIMBAL
    GimbalSKD::start(&ahrs, GIMBAL_ANGLE_INSTALLATION_MATRIX_, GIMBAL_GYRO_INSTALLATION_MATRIX_ ,THREAD_GIMBAL_SKD_PRIO);
#endif
#else
    GimbalSKD::start(THREAD_GIMBAL_SKD_PRIO);
#endif
    /// TODO: Re-enable shell commands
//    Shell::addCommands(GimbalSKD::shellCommands);
//    Shell::addFeedbackCallback(GimbalSKD::cmdFeedback);
#ifndef DEBUG_NO_SHOOT
    ShootSKD::start(THREAD_SHOOT_SKD_PRIO);
#endif
    /// TODO: Re-enable shell commands
//    Shell::addCommands(ShootSKD::shellCommands);
//    Shell::addFeedbackCallback(ShootSKD::cmdFeedback);

    MecanumChassisSKD::init(THREAD_CHASSIS_SKD_PRIO, CHASSIS_WHEEL_BASE,
                            CHASSIS_WHEEL_TREAD, CHASSIS_WHEEL_CIRCUMFERENCE);
    /// TODO: Re-enable shell commands
//    Shell::addCommands(MecanumChassisSKD::shellCommands);
//    Shell::addFeedbackCallback(MecanumChassisSKD::cmdFeedback);

    /// Start LGs
#ifndef DEBUG_NO_GIMBAL
    GimbalLG::init(THREAD_GIMBAL_LG_VISION_PRIO, THREAD_GIMBAL_LG_SENTRY_PRIO);
#endif
#ifndef DEBUG_NO_SHOOT
    ShootLG::init(SHOOT_DEGREE_PER_BULLET, true, THREAD_STUCK_DETECT_PRIO,  THREAD_SHOOT_BULLET_COUNTER_PRIO, THREAD_SHOOT_LG_VISION_PRIO);
#endif
#ifndef DEBUG_NO_CHASSIS
    ChassisLG::init(THREAD_CHASSIS_LG_PRIO, THREAD_CHASSIS_LG_PRIO, THREAD_CHASSIS_LG_PRIO);
#endif

    /// Setup Vision
//    VisionIF::init();  // must be put after initialization of GimbalSKD
//    VisionSKD::start(VISION_BASIC_CONTROL_DELAY, THREAD_VISION_SKD_PRIO);
//    VisionSKD::set_bullet_speed(VISION_DEFAULT_BULLET_SPEED);
//    Shell::addFeedbackCallback(VisionSKD::cmd_feedback);
//    Shell::addCommands(VisionSKD::shell_commands);

    /// Start Inspector and User Threads
#if !defined(DEBUG_NO_SHOOT) && !defined(DEBUG_NO_CHASSIS) && !defined(DEBUG_NO_GIMBAL)
    InspectorH::start_inspection(THREAD_INSPECTOR_PRIO);
#endif
    UserH::start(THREAD_USER_PRIO, THREAD_USER_ACTION_PRIO);

    /// Complete Period 2

    BuzzerSKD::play_sound(BuzzerSKD::sound_startup_intel);  // Now play the startup sound


    /*** ------------------------ Period 3. End of main thread ----------------------- ***/

    // Entering empty loop with low priority
#if CH_CFG_NO_IDLE_THREAD  // See chconf.h for what this #define means.
    // ChibiOS idle thread has been disabled, main() should implement infinite loop
    while (true) {}
#else
    // When vehicle() quits, the vehicle thread will somehow enter an infinite loop, so we set the
    // priority to lowest before quitting, to let other threads run normally
    chibios_rt::BaseThread::setPriority(IDLEPRIO);
#endif
    return 0;
}

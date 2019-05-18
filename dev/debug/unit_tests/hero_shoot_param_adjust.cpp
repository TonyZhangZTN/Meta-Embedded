//
// Created by liuzikai on 2019-05-18.
//

/**
 * This file reuse gimbal parameter adjustment channel.
 * Bullet loader - Yaw
 * Buller plate - Pitch
 */

#include "ch.hpp"
#include "hal.h"

#include "led.h"
#include "serial_shell.h"

#include "can_interface.h"

#include "shoot.h"

using namespace chibios_rt;

// Duplicate of motor_id_t in GimbalInterface to reduce code
unsigned const BULLET = Shoot::BULLET;
unsigned const PLATE = Shoot::PLATE;

char MOTOR_CHAR[2] = {'y', 'p'};

// Calculation interval for gimbal thread
unsigned const SHOOT_THREAD_INTERVAL = 2;    // [ms]
unsigned const SHOOT_FEEDBACK_INTERVAL = 25; // [ms]

float const MAX_VELOCITY[2] = {600, 600};  // absolute maximum, [degree/s]
int const MAX_CURRENT = 4500;  // [mA]

bool motor_enabled[2] = {false, false};

float target_bullet_num = 0;

CANInterface can1(&CAND1);

class ShootFeedbackThread : public chibios_rt::BaseStaticThread<1024> {

public:

    bool enable_bullet_feedback = false;
    bool enable_plate_feedback = false;

private:

    void main() final {

        setName("gimbal_fb");

        while (!shouldTerminate()) {

            if (enable_bullet_feedback) {
                Shell::printf("!gy,%u,%.2f,%.2f,%.2f,%.2f,%d,%d" SHELL_NEWLINE_STR,
                              SYSTIME,
                              0.0f, 0.0f,
                              Shoot::feedback[BULLET].actual_velocity, -Shoot::degree_per_bullet_ * target_bullet_num,
                              Shoot::feedback[BULLET].actual_current, Shoot::target_current[BULLET]);
            }
            if (enable_plate_feedback) {
                Shell::printf("!gp,%u,%.2f,%.2f,%.2f,%.2f,%d,%d" SHELL_NEWLINE_STR,
                              SYSTIME,
                              0.0f, 0.0f,
                              Shoot::feedback[PLATE].actual_velocity, -Shoot::degree_per_bullet_plate_ * target_bullet_num,
                              Shoot::feedback[PLATE].actual_current, Shoot::target_current[PLATE]);
            }

            sleep(TIME_MS2I(SHOOT_FEEDBACK_INTERVAL));
        }
    }

} shootFeedbackThread;


/**
 * @brief set enabled states of yaw and pitch motors
 * @param chp
 * @param argc
 * @param argv
 */
static void cmd_gimbal_enable(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 2 || (*argv[0] != '0' && *argv[0] != '1') || (*argv[1] != '0' && *argv[1] != '1')) {
        shellUsage(chp, "g_enable yaw(0/1) pitch(0/1)");
        return;
    }
    motor_enabled[0] = *argv[0] - '0';
    motor_enabled[1] = *argv[1] - '0';
}

/**
 * @brief set enabled state of friction wheels
 * @param chp
 * @param argc
 * @param argv
 */
static void cmd_gimbal_enable_fw(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 1 || (*argv[0] != '0' && *argv[0] != '1')) {
        shellUsage(chp, "g_enable_fw 0/1");
        return;
    }
    if (*argv[0] == '1') {
        Shoot::set_friction_wheels(0.8);
    } else {
        Shoot::set_friction_wheels(0);
    }
    Shoot::send_gimbal_currents();
}

/**
 * @brief set feedback enable states
 * @param chp
 * @param argc
 * @param argv
 */
static void cmd_gimbal_enable_feedback(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 2 || (*argv[0] != '0' && *argv[0] != '1') || (*argv[1] != '0' && *argv[1] != '1')) {
        shellUsage(chp, "g_enable_fb yaw(0/1) pitch(0/1)");
        return;
    }
    shootFeedbackThread.enable_bullet_feedback = *argv[0] - '0';
    shootFeedbackThread.enable_plate_feedback = *argv[1] - '0';
}


/**
 * @brief set front_angle_raw with current actual angle
 * @param chp
 * @param argc
 * @param argv
 */
static void cmd_gimbal_fix_front_angle(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 0) {
        shellUsage(chp, "g_fix");
        return;
    }
    Shoot::feedback[BULLET].reset_front_angle();
    Shoot::feedback[PLATE].reset_front_angle();

//    chprintf(chp, "!f" SHELL_NEWLINE_STR);
}

void _cmd_gimbal_clear_i_out() {
    for (int i = 0; i < 2; i++) {
        Shoot::v2i_pid[i].clear_i_out();
    }
}

/**
 * @brief set target velocity of yaw and pitch and disable pos_to_v_pid
 * @param chp
 * @param argc
 * @param argv
 */
static void cmd_gimbal_set_target_velocities(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 2) {
        shellUsage(chp, "g_set_v bullet_per plate_velocity");
        return;
    }

    target_v[0] = Shell::atof(argv[0]);
    target_v[1] = Shell::atof(argv[1]);
    _cmd_gimbal_clear_i_out();
}

/**
 * @brief set target angle of yaw and pitch and enable pos_to_v_pid
 * @param chp
 * @param argc
 * @param argv
 */
static void cmd_gimbal_set_target_angle(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 2) {
        shellUsage(chp, "g_set_angle NULL NULL");
        return;
    }

    // Do nothing
}

/**
 * @brief set pid parameters
 * @param chp
 * @param argc
 * @param argv
 */
void cmd_gimbal_set_parameters(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 7) {
        shellUsage(chp, "g_set_params bullet(0)/plate(1) NULL(0)/v_to_i(1) ki kp kd i_limit out_limit");
        chprintf(chp, "!pe" SHELL_NEWLINE_STR);  // echo parameters error
        return;
    }

    Shoot::pid_params_t bullet_v2i_params = Shoot::v2i_pid[0].get_parameters();
    Shoot::pid_params_t plate_v2i_params = Shoot::v2i_pid[1].get_parameters();

    Shoot::pid_params_t *p = nullptr;
    if (*argv[0] == '0' && *argv[1] == '1') p = &bullet_v2i_params;
    else if (*argv[0] == '1' && *argv[1] == '1') p = &plate_v2i_params;
    else {
        chprintf(chp, "!pe" SHELL_NEWLINE_STR);  // echo parameters error
        return;
    }

    *p = {Shell::atof(argv[2]),
          Shell::atof(argv[3]),
          Shell::atof(argv[4]),
          Shell::atof(argv[5]),
          Shell::atof(argv[6])};

    Shoot::change_pid_params(bullet_v2i_params, plate_v2i_params);

    chprintf(chp, "!ps" SHELL_NEWLINE_STR); // echo parameters set
}

/**
 * @brief helper function for cmd_gimbal_echo_parameters()
 */
static inline void _cmd_gimbal_echo_parameters(BaseSequentialStream *chp, Shoot::pid_params_t p) {
    chprintf(chp, "%f %f %f %f %f" SHELL_NEWLINE_STR, p.kp, p.ki, p.kd, p.i_limit, p.out_limit);
}

/**
 * @brief echo pid parameters
 * @param chp
 * @param argc
 * @param argv
 */
void cmd_gimbal_echo_parameters(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 0) {
        shellUsage(chp, "g_echo_params");
        return;
    }

    chprintf(chp, "bullet v_to_i:       ");
    _cmd_gimbal_echo_parameters(chp, Shoot::v2i_pid[BULLET].get_parameters());
    chprintf(chp, "plate v_to_i:     ");
    _cmd_gimbal_echo_parameters(chp, Shoot::v2i_pid[PLATE].get_parameters());
}

// Command lists for gimbal controller test and adjustments
ShellCommand gimbalCotrollerCommands[] = {
        {"g_enable",      cmd_gimbal_enable},
        {"g_enable_fb",   cmd_gimbal_enable_feedback},
        {"g_fix",         cmd_gimbal_fix_front_angle},
        {"g_set_v",       cmd_gimbal_set_target_velocities},
        {"g_set_angle",   cmd_gimbal_set_target_angle},
        {"g_set_params",  cmd_gimbal_set_parameters},
        {"g_echo_params", cmd_gimbal_echo_parameters},
        {"g_enable_fw",   cmd_gimbal_enable_fw},
        {nullptr,         nullptr}
};


class GimbalDebugThread : public BaseStaticThread<1024> {
protected:
    void main() final {
        setName("gimbal");
        while (!shouldTerminate()) {

            // Calculation and check
            if (motor_enabled[0] || motor_enabled[1]) {

                for (unsigned i = 0; i <= 1; i++) {

                    // Perform velocity check
                    if (Shoot::feedback[i + 2].actual_velocity > MAX_VELOCITY[i]) {
                        Shell::printf("!d%cv" SHELL_NEWLINE_STR, MOTOR_CHAR[i]);
                        motor_enabled[i] = false;
                        continue;
                    }

                    // Calculate from velocity to current
                    Shoot::calc_bullet_loader((Gimbal::motor_id_t) i, actual_velocity_, Gimbal::target_velocity[i]);
                    // NOTE: Gimbal::target_velocity[i] is either calculated or filled (see above)


                    // Perform current check
                    if (Gimbal::target_current[i] > MAX_CURRENT || Gimbal::target_current[i] < -MAX_CURRENT) {
                        Shell::printf("!d%cc" SHELL_NEWLINE_STR, MOTOR_CHAR[i]);
                        motor_enabled[i] = false;
                        continue;
                    }
                }

            }

            // This two operations should be after calculation since motor can get disabled if check failed
            // This two operations should always perform, instead of being put in a 'else' block
            if (!motor_enabled[YAW]) Gimbal::target_current[YAW] = 0;
            if (!motor_enabled[PITCH]) Gimbal::target_current[PITCH] = 0;

            // Send currents
            GimbalInterface::send_gimbal_currents();

            sleep(TIME_MS2I(GIMBAL_THREAD_INTERVAL));
        }
    }
} gimbalThread;


int main(void) {

    halInit();
    System::init();
    LED::all_off();
    Shell::start(HIGHPRIO);
    Shell::addCommands(gimbalCotrollerCommands);

    can1.start(HIGHPRIO - 1);
    MPU6500::start(HIGHPRIO - 2);
    chThdSleepMilliseconds(10);
    Gimbal::init(&can1, GIMBAL_YAW_FRONT_ANGLE_RAW, GIMBAL_PITCH_FRONT_ANGLE_RAW);

    gimbalFeedbackThread.start(NORMALPRIO - 1);
    gimbalThread.start(NORMALPRIO);

    // See chconf.h for what this #define means.
#if CH_CFG_NO_IDLE_THREAD
    // ChibiOS idle thread has been disabled,
    // main() should implement infinite loop
    while (true) {}
#else
    // When main() quits, the main thread will somehow enter an infinite loop, so we set the priority to lowest
    // before quitting, to let other threads run normally
    BaseThread::setPriority(1);
#endif
    return 0;

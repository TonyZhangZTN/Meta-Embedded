//
// Created by liuzikai on 2019-05-17.
//

/**
 * This unit test reuse unit for gimbal.
 */
#include "ch.hpp"
#include "hal.h"
#include "led.h"
#include "serial_shell.h"
#include "can_interface.h"
#include "common_macro.h"
#include "buzzer.h"

#include "engineer_elevator_interface.h"
#include "engineer_elevator_skd.h"

using namespace chibios_rt;

unsigned const R = EngineerElevatorIF::R;
unsigned const L = EngineerElevatorIF::R;

unsigned const ELEVATOR_FEEDBACK_INTERVAL = 25; // [ms]

int const MAX_VELOCITY = {40960};  // absolute maximum, [qc/s]
int const MAX_CURRENT = 6000;  // [mA]

bool elevator_enabled = false;  // for out

bool enable_a2v_pid = false;

float target_angle = 0;
float target_v = 0;

CANInterface can2(&CAND2);


class ElevatorFeedbackThread : public chibios_rt::BaseStaticThread<1024> {

public:

    bool enable_right_feedback = false;
    bool enable_left_feedback = false;

private:

    void main() final {

        setName("elevator_fb");

        while (!shouldTerminate()) {

            if (enable_right_feedback) {
                Shell::printf("!gy,%u,%.2f,%.2f,%.2f,%.2f,%d,%d" SHELL_NEWLINE_STR,
                              SYSTIME,
                              EngineerElevatorIF::elevatorMotor[R].present_angle, EngineerElevatorSKD::target_height * ANGLE_HEIGHT_RATIO,
                              EngineerElevatorIF::elevatorMotor[R].actual_velocity, EngineerElevatorSKD::target_velocity[0],
                              EngineerElevatorIF::elevatorMotor[R].actual_current, EngineerElevatorIF::elevatorMotor[R].target_current);
            }
            if (enable_left_feedback) {
                Shell::printf("!gp,%u,%.2f,%.2f,%.2f,%.2f,%d,%d" SHELL_NEWLINE_STR,
                              SYSTIME,
                              EngineerElevatorIF::elevatorMotor[L].present_angle, EngineerElevatorSKD::target_height * ANGLE_HEIGHT_RATIO,
                              EngineerElevatorIF::elevatorMotor[L].actual_velocity, EngineerElevatorSKD::target_velocity[1],
                              EngineerElevatorIF::elevatorMotor[L].actual_current, EngineerElevatorIF::elevatorMotor[L].target_current);
            }

            sleep(TIME_MS2I(ELEVATOR_FEEDBACK_INTERVAL));
        }
    }

} elevatorFeedbackThread;


static void cmd_elevator_enable(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 2) {
        shellUsage(chp, "g_enable (0/1) NULL");
        return;
    }
    EngineerElevatorSKD::elevator_enable(*argv[0] - '0');
}

static void cmd_elevator_enable_fw(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 1) {
        shellUsage(chp, "g_enable_fw DELETED");
        return;
    }
    // Do nothing
}

static void cmd_elevator_enable_feedback(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 2) {
        shellUsage(chp, "g_enable_fb DELETED");
        return;
    }
    // Do nothing
}

static void cmd_elevator_fix_front_angle(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 0) {
        shellUsage(chp, "g_fix");
        return;
    }
    for (unsigned i = 0; i < 2; i++)
        EngineerElevatorIF::elevatorMotor[i].clear_accmulate_angle();
}

void cmd_elevator_set_target_velocities(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 2) {
        shellUsage(chp, "g_set_v target_velocity NULL");
        return;
    }

    EngineerElevatorSKD::target_velocity[0] = EngineerElevatorSKD::target_velocity[1] = Shell::atof(argv[0]);
    enable_a2v_pid = false;
}

static void cmd_elevator_set_target_angle(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 2) {
        shellUsage(chp, "g_set_angle target_angle NULL");
        return;
    }

    target_angle = Shell::atof(argv[0]);
    enable_a2v_pid = true;
}

/**
 * @brief set pid parameters
 * @param chp
 * @param argc
 * @param argv
 */
void cmd_elevator_set_parameters(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 7) {
        shellUsage(chp, "g_set_params yaw(0)/pitch(1) angle_to_v(0)/v_to_i(0) ki kp kd i_limit out_limit");
        chprintf(chp, "!pe" SHELL_NEWLINE_STR);  // echo parameters error
        return;
    }

    Elevator::pid_params_t a2v_params = Elevator::a2v_pid[FR].get_parameters();
    Elevator::pid_params_t v2i_params = Elevator::v2i_pid[FR].get_parameters();

    Elevator::pid_params_t *p = nullptr;
    if (*argv[0] == '0' && *argv[1] == '0') p = &a2v_params;
    else if (*argv[0] == '0' && *argv[1] == '1') p = &v2i_params;
    else {
        chprintf(chp, "!pe" SHELL_NEWLINE_STR);  // echo parameters error
        return;
    }

    *p = {Shell::atof(argv[2]),
          Shell::atof(argv[3]),
          Shell::atof(argv[4]),
          Shell::atof(argv[5]),
          Shell::atof(argv[6])};

    Elevator::change_pid_params(a2v_params, v2i_params);

    chprintf(chp, "!ps" SHELL_NEWLINE_STR); // echo parameters set
}

/**
 * @brief helper function for cmd_elevator_echo_parameters()
 */
static inline void _cmd_elevator_echo_parameters(BaseSequentialStream *chp, Elevator::pid_params_t p) {
    chprintf(chp, "%f %f %f %f %f" SHELL_NEWLINE_STR, p.kp, p.ki, p.kd, p.i_limit, p.out_limit);
}

/**
 * @brief echo pid parameters
 * @param chp
 * @param argc
 * @param argv
 */
void cmd_elevator_echo_parameters(BaseSequentialStream *chp, int argc, char *argv[]) {
    (void) argv;
    if (argc != 0) {
        shellUsage(chp, "g_echo_params");
        return;
    }

    chprintf(chp, "angle_to_v:   ");
    _cmd_elevator_echo_parameters(chp, Elevator::a2v_pid[FR].get_parameters());
    chprintf(chp, "v_to_i:       ");
    _cmd_elevator_echo_parameters(chp, Elevator::v2i_pid[FR].get_parameters());
}

// Command lists for elevator controller test and adjustments
ShellCommand elevatorCotrollerCommands[] = {
        {"g_enable",      cmd_elevator_enable},
        {"g_enable_fb",   cmd_elevator_enable_feedback},
        {"g_fix",         cmd_elevator_fix_front_angle},
        {"g_set_v",       cmd_elevator_set_target_velocities},
        {"g_set_angle",   cmd_elevator_set_target_angle},
        {"g_set_params",  cmd_elevator_set_parameters},
        {"g_echo_params", cmd_elevator_echo_parameters},
        {"g_enable_fw",   cmd_elevator_enable_fw},
        {nullptr,         nullptr}
};


class ElevatorDebugThread : public BaseStaticThread<1024> {
protected:
    void main() final {
        setName("elevator");
        while (!shouldTerminate()) {

            // Calculation and check
            if (elevator_enabled) {

                for (unsigned i = 0; i < MOTOR_COUNT; i++) {

                    if (enable_a2v_pid) {
                        // Calculate from angle to velocity
                        Elevator::calc_a2v_((Elevator::motor_id_t) i, Elevator::feedback[i].accmulate_angle, target_angle);
                    } else {
                        // Directly fill the target velocity
                        Elevator::target_velocity[i] = target_v;
                    }

                    // Perform velocity check
                    if (Elevator::feedback[i].actual_velocity > MAX_VELOCITY ||
                        Elevator::feedback[i].actual_velocity < -MAX_VELOCITY) {
                        Shell::printf("!dyv" SHELL_NEWLINE_STR);
                        elevator_enabled = false;
                        continue;
                    }

                    // Calculate from velocity to current
                    Elevator::calc_v2i_((Elevator::motor_id_t) i, Elevator::feedback[i].actual_velocity, Elevator::target_velocity[i]);
                    // NOTE: Elevator::target_velocity[i] is either calculated or filled (see above)


                    // Perform current check
                    if (Elevator::target_current[i] > MAX_CURRENT || Elevator::target_current[i] < -MAX_CURRENT) {
                        Shell::printf("!dyc" SHELL_NEWLINE_STR);
                        elevator_enabled = false;
                        continue;
                    }
                }

            }

            // This operation should be after calculation since motor can get disabled if check failed
            // This operation should always perform, instead of being put in a 'else' block
            if (!elevator_enabled) {
                for (unsigned i = 0; i < MOTOR_COUNT; i++) {
                    Elevator::target_current[i] = 0;
                }
            }
            

            // Send currents
            Elevator::send_elevator_currents();

            sleep(TIME_MS2I(ELEVATOR_THREAD_INTERVAL));
        }
    }
} elevatorThread;


int main(void) {

    halInit();
    System::init();
    LED::all_off();
    Shell::start(HIGHPRIO);
    Shell::addCommands(elevatorCotrollerCommands);

    can1.start(HIGHPRIO - 1);
    can2.start(HIGHPRIO - 2);
    chThdSleepMilliseconds(10);
    Elevator::init(&can2);

    elevatorFeedbackThread.start(NORMALPRIO - 1);
    elevatorThread.start(NORMALPRIO);

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
}
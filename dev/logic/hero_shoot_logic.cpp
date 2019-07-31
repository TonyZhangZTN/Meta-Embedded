//
// Created by 钱晨 on 2019-07-03.
//

#include "hero_shoot_logic.h"

#include "shell.h"
#include "referee_interface.h"
#include "shoot_scheduler.h"

float HeroShootLG::loader_angle_per_bullet = 0.0f;
float HeroShootLG::plate_angle_per_bullet = 0.0f;
float HeroShootLG::loader_target_angle = 0.0f;
float HeroShootLG::plate_target_angle = 0.0f;

HeroShootLG::bullet_type_t HeroShootLG::bullet_type = TRANSPARENT;
int HeroShootLG::bullet_in_tube = 0;
bool HeroShootLG::should_shoot = false;

HeroShootLG::motor_state_t HeroShootLG::loader_state = STOP;
HeroShootLG::motor_state_t HeroShootLG::plate_state = STOP;

HeroShootLG::LoaderStuckDetectorThread HeroShootLG::loaderStuckDetector;
HeroShootLG::PlateStuckDetectorThread HeroShootLG::plateStuckDetector;
HeroShootLG::LoaderThread HeroShootLG::loaderThread;
HeroShootLG::PlateThread HeroShootLG::plateThread;

void HeroShootLG::init(float loader_angle_per_bullet_, float plate_angle_per_bullet_,
                       tprio_t loader_thread_prio, tprio_t plate_thread_prio,
                       tprio_t loader_stuck_detector_prio, tprio_t plate_stuck_detector_prio) {

    // Initialize parameters
    loader_angle_per_bullet = loader_angle_per_bullet_;
    plate_angle_per_bullet = plate_angle_per_bullet_;

    // Clear the bullet angle
    ShootSKD::reset_loader_accumulated_angle();
    ShootSKD::reset_plate_accumulated_angle();

    ShootSKD::set_mode(ShootSKD::LIMITED_SHOOTING_MODE);

    loaderThread.start(loader_thread_prio);
    plateThread.start(plate_thread_prio);
    loaderStuckDetector.start(loader_stuck_detector_prio);
    plateStuckDetector.start(plate_stuck_detector_prio);

}

bool HeroShootLG::get_loader_exit_status() {
    if (bullet_type == TRANSPARENT) {
        return !(bool) palReadPad(GPIOF, GPIOF_PIN0);
    } else {
        return (bool) palReadPad(GPIOF, GPIOF_PIN0);
    }
}

bool HeroShootLG::get_plate_exit_status() {
    if (bullet_type == TRANSPARENT) {
        return !(bool) palReadPad(GPIOF, GPIOF_PIN1);
    } else {
        return (bool) palReadPad(GPIOF, GPIOF_PIN1);
    }
}

void HeroShootLG::shoot() {
    if (loader_state == STOP && get_loader_exit_status()) {
        should_shoot = true;
    }
}

void HeroShootLG::set_friction_wheels(float duty_cycle) {
    ShootSKD::set_friction_wheels(duty_cycle);
    Referee::set_client_light(USER_CLIENT_FW_STATE_LIGHT, (duty_cycle != 0));
    // Sending client data will be complete by higher level thread
}

float HeroShootLG::get_friction_wheels_duty_cycle() {
    return ShootSKD::get_friction_wheels_duty_cycle();
}

void HeroShootLG::force_stop() {
    ShootSKD::set_mode(ShootSKD::FORCED_RELAX_MODE);
}

void HeroShootLG::LoaderStuckDetectorThread::main() {
    setName("StuckDetector1");

    int stuck_pend_time = 0;

    while (!shouldTerminate()) {

        if (loader_state == LOADING) {
            if (ShootSKD::get_loader_target_current() > LOADER_STUCK_THRESHOLD_CURRENT &&
                ShootSKD::get_loader_actual_velocity() < LOADER_STUCK_THRESHOLD_VELOCITY) {

                stuck_pend_time++;  // Back up to ample space

            } else {

                stuck_pend_time = 0;
            }
        }

        if (stuck_pend_time > 200) {
            loader_state = STUCK;
            ShootSKD::set_loader_target_angle(ShootSKD::get_loader_accumulated_angle() - STUCK_REVERSE_ANGLE);

            sleep(TIME_MS2I(STUCK_REVERSE_TIME));

            loader_state = LOADING;
            ShootSKD::set_loader_target_angle(loader_target_angle);

            stuck_pend_time = 0;
        }

        sleep(TIME_MS2I(STUCK_DETECTOR_THREAD_INTERVAL));
    }
}

void HeroShootLG::PlateStuckDetectorThread::main() {

    setName("StuckDetector2");

    int stuck_pend_time = 0;

    while (!shouldTerminate()) {

        if (plate_state == LOADING) {
            if (ShootSKD::get_plate_target_current() > PLATE_STUCK_THRESHOLD_CURRENT &&
                ShootSKD::get_plate_actual_velocity() < PLATE_STUCK_THRESHOLD_VELOCITY) {

                stuck_pend_time++;  // Back up to ample space

            } else {

                stuck_pend_time = 0;
            }
        }

        if (stuck_pend_time > 200) {
            plate_state = STUCK;
            ShootSKD::set_plate_target_angle(ShootSKD::get_plate_accumulated_angle() - STUCK_REVERSE_ANGLE);

            sleep(TIME_MS2I(STUCK_REVERSE_TIME));

            plate_state = LOADING;
            ShootSKD::set_plate_target_angle(plate_target_angle);

            stuck_pend_time = 0;
        }

        sleep(TIME_MS2I(STUCK_DETECTOR_THREAD_INTERVAL));
    }
}

void HeroShootLG::LoaderThread::main() {

    setName("HeroShootLoader");

    while (!shouldTerminate()) {

        if (loader_state != STUCK) {

            if (ABS_IN_RANGE(loader_target_angle - ShootSKD::get_loader_accumulated_angle(), LOADER_THREAD_INTERVAL)) {
                loader_state = STOP;
            } else if (ABS_IN_RANGE(loader_target_angle - ShootSKD::get_loader_accumulated_angle(),
                                    LOADER_THREAD_INTERVAL)) {
                loader_state = LOADING;
            }

            if (loader_state == STOP) {
                if (should_shoot || !get_loader_exit_status()) {
                    if (should_shoot && get_loader_exit_status()) {
                        should_shoot = false;
                        bullet_in_tube--;
                    }
                    loader_target_angle += loader_angle_per_bullet;
                    ShootSKD::set_loader_target_angle(loader_target_angle);
                }
            }

        }

        sleep(TIME_MS2I(LOADER_THREAD_INTERVAL));
    }

}

void HeroShootLG::PlateThread::main() {

    setName("HeroShootPlate");

    while (!shouldTerminate()) {

        if (plate_state != STUCK) {

            if (!last_plate_exit_status && get_plate_exit_status()) {
                bullet_in_tube++;
            }
            last_plate_exit_status = get_plate_exit_status();

            if (ABS_IN_RANGE(plate_target_angle - ShootSKD::get_plate_accumulated_angle(), PLATE_REACH_TARGET_RANGE)) {
                plate_state = STOP;
            } else if (ABS_IN_RANGE(plate_target_angle - ShootSKD::get_plate_accumulated_angle(),
                                    PLATE_REACH_TARGET_RANGE)) {
                plate_state = LOADING;
            }

            if (plate_state == STOP) {
                if (bullet_in_tube < 3) {
                    plate_target_angle += plate_angle_per_bullet;
                    ShootSKD::set_plate_target_angle(plate_target_angle);
                }
            }

        }

        sleep(TIME_MS2I(PLATE_THREAD_INTERVAL));
    }

}

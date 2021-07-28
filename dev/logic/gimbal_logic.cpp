//
// Created by liuzikai on 2019-06-26.
//

/**
 * @file    gimbal_logic.cpp
 * @brief   By-passing logic-level module to control GimbalLG
 *
 * @addtogroup gimbal
 * @{
 */

#include "gimbal_logic.h"

GimbalLG::action_t GimbalLG::action = FORCED_RELAX_MODE;
GimbalLG::VisionControlThread GimbalLG::vision_control_thread;
GimbalLG::SentryControlThread GimbalLG::sentry_control_thread;

void GimbalLG::init(tprio_t vision_control_thread_prio, tprio_t sentry_control_thread_prio) {
    vision_control_thread.start(vision_control_thread_prio);
    sentry_control_thread.start(sentry_control_thread_prio);
}

GimbalLG::action_t GimbalLG::get_action() {
    return action;
}

void GimbalLG::set_action(GimbalLG::action_t value) {
    if (value == action) return;  // avoid repeating setting target_theta, etc.

    action = value;
    if (action == FORCED_RELAX_MODE) {
        GimbalSKD::set_mode(GimbalSKD::FORCED_RELAX_MODE);
    } else {
        GimbalSKD::set_mode(GimbalSKD::ENABLED_MODE);
    }
}

void GimbalLG::set_target(float yaw_target_angle, float pitch_target_angle) {
    if (action != FORCED_RELAX_MODE && action != VISION_MODE) {
        GimbalSKD::set_target_angle(yaw_target_angle, pitch_target_angle, 0);
    } else {
        LOG_ERR("GimbalLG - set_target(): invalid mode");
    }
}

float GimbalLG::get_actual_angle(GimbalBase::motor_id_t motor) {
    return GimbalSKD::get_actual_angle(motor);
}

float GimbalLG::get_relative_angle(GimbalBase::motor_id_t motor) {
    return GimbalSKD::get_relative_angle(motor);
}

float GimbalLG::get_current_target_angle(GimbalBase::motor_id_t motor) {
    return GimbalSKD::get_target_angle(motor);
}

void GimbalLG::separate_pitch() {
    float pitch = GimbalSKD::get_actual_angle(PITCH) + GimbalSKD::get_actual_angle(SUB_PITCH);
    float target_pitch = pitch;
    float flight_time;
    Trajectory::compensate_for_gravity(target_pitch, *GimbalIF::lidar_dist, 10, flight_time);
    GimbalSKD::separate_pitch(target_pitch - pitch);
}

void GimbalLG::VisionControlThread::main() {
    setName("GimbalLG_Vision");

    chEvtRegisterMask(&Vision::gimbal_updated_event, &vision_listener, VISION_UPDATED_EVENT_MASK);

    while (!shouldTerminate()) {

        chEvtWaitAny(VISION_UPDATED_EVENT_MASK);

        if (action == VISION_MODE) {
            float yaw, pitch;
            bool can_reach_the_target;

            chSysLock();  /// --- ENTER S-Locked state. DO NOT use LOG, printf, non S/I-Class functions or return ---
            {
                can_reach_the_target = Vision::get_gimbal_target_angles_S(yaw, pitch);
            }
            chSysUnlock();  /// --- EXIT S-Locked state ---

            if (can_reach_the_target) {
                GimbalSKD::set_target_angle(yaw, pitch, 0);
            }  // otherwise, keep current target angles
        }
    }
}

void GimbalLG::SentryControlThread::main() {
    setName("GimbalLG_Sentry");

    while (!shouldTerminate()) {

        if (action == SENTRY_MODE) {
            float yaw = GimbalSKD::get_target_angle(YAW) + 0.05f;
            time_ticket += 0.05f;
            float pitch = sin(time_ticket / 180 * PI);
            GimbalSKD::set_target_angle(yaw, pitch, 0);
        }
        sleep(TIME_MS2I(SENTRY_THREAD_INTERVAL));
    }
}

/** @} */
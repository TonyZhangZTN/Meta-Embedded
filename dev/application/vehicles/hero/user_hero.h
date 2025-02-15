//
// Created by liuzikai on 2019-06-25.
//

#ifndef META_INFANTRY_USER_HERO_H
#define META_INFANTRY_USER_HERO_H

#include "ch.hpp"

#include "remote_interpreter.h"
#include "referee_interface.h"
#include "capacitor_interface.h"

#include "gimbal_logic.h"
#include "shoot_logic.h"
#include "chassis_logic.h"
#include "vision_scheduler.h"

#include "inspector_hero.h"

// Added by TonyZhangZTN for debug
//#define DEBUG_NO_CHASSIS
#define DEBUG_NO_GIMBAL
//#define DEBUG_NO_SHOOT
#define DEBUG_NO_VISION
class UserH {

public:

    enum pitch_separate_mode_t {
        IN_ACTIVE,
        SEPARATED,
        MERGED
    };
    static void start(tprio_t user_thd_prio, tprio_t user_action_thd_prio);

private:

    /// Gimbal Config
    static float gimbal_rc_yaw_max_speed;  // [degree/s]
    static float gimbal_pc_yaw_sensitivity[];  // [Ctrl, Normal, Shift] [degree/s]

    static float gimbal_pc_pitch_sensitivity[];   // rotation speed when mouse moves fastest [degree/s]
    static float gimbal_pitch_min_angle; // down range for pitch [degree]
    static float gimbal_pitch_max_angle; //  up range for pitch [degree]

    /// Chassis Config
    static float base_power;             // [w]
    static float base_v_forward;        // [mm/s]
    static float chassis_v_left_right;  // [mm/s]
    static float Base_left_right_power; // [w]
    static  float Base_left_right;      // [mm/s]
    static float chassis_v_forward;     // [mm/s]
    static float chassis_v_backward;    // [mm/s]

    static float chassis_pc_shift_ratio;  // 150% when Shift is pressed
    static float chassis_pc_ctrl_ratio;    // 50% when Ctrl is pressed

    static float shoot_launch_left_count;
    static float shoot_launch_right_count;

    static float shoot_feed_rate;
    static float shoot_fw_speed[3];

    static bool mag_status;

    /// Helpers

    /// Runtime variables

    static float gimbal_yaw_target_angle_;
    static float gimbal_pc_pitch_target_angle_;
    static float gimbal_pc_sub_pitch_target_angle_;

    static pitch_separate_mode_t pitch_separated;

    /// User Thread
    static constexpr unsigned USER_THREAD_INTERVAL = 7;  // [ms]
    class UserThread : public chibios_rt::BaseStaticThread<512> {
        void main() final;
    };

    static UserThread userThread;


    /// User Action Thread
    class UserActionThread : public chibios_rt::BaseStaticThread<512> {

        /// Runtime variables
        event_listener_t s_change_listener;
        static constexpr eventmask_t S_CHANGE_EVENTMASK = (1U << 0U);

        event_listener_t mouse_press_listener;
        static constexpr eventmask_t MOUSE_PRESS_EVENTMASK = (1U << 1U);

        event_listener_t mouse_release_listener;
        static constexpr eventmask_t MOUSE_RELEASE_EVENTMASK = (1U << 2U);

        event_listener_t key_press_listener;
        static constexpr eventmask_t KEY_PRESS_EVENTMASK = (1U << 3U);

        event_listener_t key_release_listener;
        static constexpr eventmask_t KEY_RELEASE_EVENTMASK = (1U << 4U);

        void main() final;
    };

    static UserActionThread userActionThread;

    /// Friend Configure Functions
    friend void gimbal_get_config(BaseSequentialStream *chp, int argc, char *argv[]);
    friend void gimbal_set_config(BaseSequentialStream *chp, int argc, char *argv[]);
    friend void chassis_get_config(BaseSequentialStream *chp, int argc, char *argv[]);
    friend void chassis_set_config(BaseSequentialStream *chp, int argc, char *argv[]);
    friend void shoot_get_config(BaseSequentialStream *chp, int argc, char *argv[]);
    friend void shoot_set_config(BaseSequentialStream *chp, int argc, char *argv[]);

};


#endif //META_INFANTRY_USER_HERO_H

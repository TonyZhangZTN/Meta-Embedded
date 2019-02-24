//
// Created by Ye Anbang on 2019/2/2.
//

#ifndef META_INFANTRY_FETCH_BULLET_H
#define META_INFANTRY_FETCH_BULLET_H

#include "ch.hpp"
#include "hal.h"
#include "hal_pal.h"
#include "can_interface.h"
#include "port_to_string.h"

static uint16_t rotate_current = 30;  //TODO change it
static uint16_t release_current = 30;  //TODO change it
static uint16_t inactive_current = 30;  //TODO: change it

// TODO: make sure that there is no CAN conflicts
/**
 * @pre rotation motor CAN ID = 5
 * @pre clamp connect to PH2
 */
class RoboticArm {

public:

    static uint16_t get_rotation_motor_angle_raw();

    enum clamp_status_t {
        CLAMP_RELAX = PAL_LOW,
        CLAMP_CLAMPED = PAL_HIGH
    };

    static clamp_status_t get_clamp_status();

    static void clamp_action(clamp_status_t target_status);

    static void set_rotation_motor_target_current(int target_current);

    static bool send_rotation_motor_target_current();

    static void init(CANInterface *can_interface);

private:

    static clamp_status_t _clamp_status; // local storage
    static uint16_t rotation_motor_angle_raw;
    static int rotation_motor_target_current;

    static void process_rotation_motor_feedback(CANRxFrame const*rxmsg);

    static CANInterface *can;

    friend CANInterface;

};

class FetchBulletThread : public chibios_rt::BaseStaticThread<128> {
private:
    ioportid_t _ioportid;
    ioportmask_t _ioportmask;

protected:
    void main(void) override;
public:

    typedef enum {
        INACTIVE,   //initial state, ready for holding a box
        MACHINE_HAND_CLAMP, //clamp the machine hand
        ROTATE, //rotate the hand
        RELEASE,    //release all bullets
        RESTORE     //return to inactive state
    } activity_mode;

    uint16_t delay1;
    uint16_t delay2;
    uint16_t delay3;
    CANInterface* can;
    uint16_t mode;
    FetchBulletThread(ioportid_t ioportid, ioportmask_t ioportmask, uint16_t delay_1,
            uint16_t delay_2, uint16_t delay_3, CANInterface *can_interface);
    void fetch_once();
};


#endif //META_INFANTRY_FETCH_BULLET_H

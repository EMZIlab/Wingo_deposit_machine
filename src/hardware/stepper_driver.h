#pragma once

#include <stdint.h>

typedef enum { STP_UNINIT=0, STP_READY, STP_ENABLED, STP_MOVING, STP_HOMING, STP_FAULT } stepper_state_t;


typedef struct stepper_motor {
    const char *gpiochip;
    uint32_t stp_per_rev;
    uint32_t pulse_width_us;
    uint8_t pul_pin;
    uint8_t dir_pin;
    uint8_t enable_pin;
    uint8_t home_pin;
    uint8_t home_active_level;
    uint8_t dir_invert;
    uint8_t en_active_level;
    uint8_t homed;
    int32_t target_pos_stp;
    uint32_t target_speed_sps;
    uint32_t target_acc_sps2;
    uint32_t cur_speed_sps;
    int32_t cur_pos_stp;
    uint32_t next_step_us;
    stepper_state_t state;
}stepper_motor;

int stepper_init (stepper_motor *motor);
int stepper_enable (stepper_motor *motor);
int stepper_disable (stepper_motor *motor);
void stepper_set_pos (stepper_motor *motor, int32_t pos_stp);
float stepper_get_pos_deg(const stepper_motor *motor);
int stepper_start_homing (stepper_motor *motor, uint32_t speed_sps, uint32_t acc_sps2, int8_t dir);
int stepper_start_move_abs (stepper_motor *motor, int32_t abs_stp, uint32_t speed_sps, uint32_t acc_sps2);
int stepper_start_move_rel (stepper_motor *motor, int32_t delta_stp, uint32_t speed_sps, uint32_t acc_sps2);
void stepper_update (stepper_motor *motor, uint32_t now_us);
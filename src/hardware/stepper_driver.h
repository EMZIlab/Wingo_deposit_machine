// File: src/hardware/stepper_driver.h
#pragma once
#include <stdint.h>

typedef enum {
    STP_UNINIT=0,
    STP_READY,
    STP_ENABLED,
    STP_MOVING,
    STP_HOMING,
    STP_FAULT
} stepper_state_t;

typedef struct stepper_motor {
    const char *gpiochip;

    uint32_t stp_per_rev;       // informational
    uint32_t pulse_width_us;    // STEP high time

    uint8_t  pul_pin;
    uint8_t  dir_pin;
    uint8_t  enable_pin;

    uint8_t  home_pin;
    uint8_t  home_active_level; // 0 or 1

    uint8_t  dir_invert;
    uint8_t  en_active_level;

    uint8_t  homed;

    int32_t  target_pos_stp;
    uint32_t target_speed_sps;
    uint32_t target_acc_sps2;

    uint32_t cur_speed_sps;     // integer speed used for period calc
    uint64_t cur_speed_fp;      // steps/s * 1e6
    int32_t  cur_pos_stp;

    stepper_state_t state;

    // timing
    uint32_t last_speed_us;
    uint32_t enabled_at_us;
    uint8_t  need_dir_setup;

    // pulse edge scheduler
    uint8_t  step_level;
    uint32_t next_edge_us;

} stepper_motor;

int   stepper_init(stepper_motor *motor);
int   stepper_enable(stepper_motor *motor);
int   stepper_disable(stepper_motor *motor);

void  stepper_set_pos(stepper_motor *motor, int32_t pos_stp);
float stepper_get_pos_deg(const stepper_motor *motor);

int   stepper_start_homing(stepper_motor *motor, uint32_t speed_sps, uint32_t acc_sps2, int8_t dir);
int   stepper_start_move_abs(stepper_motor *motor, int32_t abs_stp, uint32_t speed_sps, uint32_t acc_sps2);
int   stepper_start_move_rel(stepper_motor *motor, int32_t delta_stp, uint32_t speed_sps, uint32_t acc_sps2);

// call as often as possible, pass monotonic time in microseconds
void  stepper_update(stepper_motor *motor, uint32_t now_us);

/*
    Debug helper: read home/limit switch.
    raw_out: the direct GPIO read (0/1)
    active_out: 1 if raw == motor->home_active_level else 0
    Returns:
      0  ok
     -1  home line not initialized
     -2  gpiod read error
*/
int stepper_home_read(const stepper_motor *motor, int *raw_out, int *active_out);

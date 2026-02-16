#include "stepper_driver.h"

#include <gpiod.h>
#include <time.h>

static struct {
    stepper_motor *m;
    struct gpiod_chip *chip;
    struct gpiod_line *step;
    struct gpiod_line *dir;
    struct gpiod_line *en;
} g = {0};

static void nsleep_us(uint32_t us){
    struct timespec ts;
    ts.tv_sec  = (time_t)(us / 1000000u);
    ts.tv_nsec = (long)((us % 1000000u) * 1000u);
    nanosleep(&ts, 0);
}

int stepper_init(stepper_motor *m){
    if (!m || !m->gpiochip) return -1;

    g.chip = gpiod_chip_open(m->gpiochip);
    if (!g.chip) return -2;

    g.step = gpiod_chip_get_line(g.chip, m->pul_pin);
    g.dir  = gpiod_chip_get_line(g.chip, m->dir_pin);
    g.en   = gpiod_chip_get_line(g.chip, m->enable_pin);
    if (!g.step || !g.dir || !g.en) return -3;

    if (gpiod_line_request_output(g.step, "stp_step", 0) < 0) return -4;
    if (gpiod_line_request_output(g.dir,  "stp_dir",  0) < 0) return -5;

    int en_idle = (m->en_active_level ? 0 : 1); // disabled
    if (gpiod_line_request_output(g.en, "stp_en", en_idle) < 0) return -6;

    m->cur_pos_stp = 0;
    m->cur_speed_sps = 0;
    m->target_pos_stp = 0;
    m->next_step_us = 0;
    m->state = STP_READY;

    g.m = m;
    return 0;
}

int stepper_enable(stepper_motor *m){
    if (!m || !g.en || g.m != m) return -1;
    gpiod_line_set_value(g.en, m->en_active_level ? 1 : 0);
    m->state = STP_ENABLED;
    return 0;
}

int stepper_disable(stepper_motor *m){
    if (!m || !g.en || g.m != m) return -1;
    gpiod_line_set_value(g.en, m->en_active_level ? 0 : 1);
    m->state = STP_READY;
    return 0;
}

void stepper_set_pos(stepper_motor *m, int32_t pos_stp){
    if (!m) return;
    m->cur_pos_stp = pos_stp;
}

float stepper_get_pos_deg(const stepper_motor *m){
    if (!m || m->stp_per_rev == 0) return 0.0f;
    return (float)m->cur_pos_stp * (360.0f / (float)m->stp_per_rev);
}

int stepper_start_move_abs(stepper_motor *m, int32_t abs_stp, uint32_t speed_sps, uint32_t acc_sps2){
    (void)acc_sps2;
    if (!m || speed_sps == 0) return -1;

    m->target_pos_stp = abs_stp;
    m->target_speed_sps = speed_sps;
    m->target_acc_sps2 = 0;
    m->state = STP_MOVING;

    int32_t delta = m->target_pos_stp - m->cur_pos_stp;
    int dir = (delta >= 0) ? 1 : 0;
    if (m->dir_invert) dir ^= 1;
    gpiod_line_set_value(g.dir, dir);

    m->next_step_us = 0; // force immediate step on next update
    return 0;
}

int stepper_start_move_rel(stepper_motor *m, int32_t delta_stp, uint32_t speed_sps, uint32_t acc_sps2){
    return stepper_start_move_abs(m, m->cur_pos_stp + delta_stp, speed_sps, acc_sps2);
}

int stepper_start_homing(stepper_motor *m, uint32_t speed_sps, uint32_t acc_sps2, int8_t dir){
    (void)m; (void)speed_sps; (void)acc_sps2; (void)dir;
    return -1; // not implemented in this minimal version
}

void stepper_update(stepper_motor *m, uint32_t now_us){
    if (!m || g.m != m) return;
    if (m->state != STP_MOVING) return;

    int32_t delta = m->target_pos_stp - m->cur_pos_stp;
    if (delta == 0){
        m->state = STP_ENABLED;
        return;
    }

    uint32_t period_us = 1000000u / (m->target_speed_sps ? m->target_speed_sps : 1u);

    if (m->next_step_us == 0 || (uint32_t)(now_us - m->next_step_us) >= period_us){
        int step_dir = (delta > 0) ? +1 : -1;

        gpiod_line_set_value(g.step, 1);
        if (m->pulse_width_us) nsleep_us(m->pulse_width_us);
        gpiod_line_set_value(g.step, 0);

        m->cur_pos_stp += step_dir;
        m->next_step_us = now_us;
    }
}

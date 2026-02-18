// File: src/hardware/stepper_driver.c
#include "stepper_driver.h"

#include <gpiod.h>
#include <time.h>

#define DIR_SETUP_US        10u
#define MAX_SPS             200000u
#define ENABLE_SETTLE_US    200000u

static struct {
    stepper_motor     *m;
    struct gpiod_chip *chip;
    struct gpiod_line *step;
    struct gpiod_line *dir;
    struct gpiod_line *en;
    struct gpiod_line *home;
} g = {0};

static uint32_t now_us_local(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = (uint64_t)(int64_t)ts.tv_sec * 1000000u
                + (uint64_t)(int64_t)ts.tv_nsec / 1000u;
    return (uint32_t)us;
}

static uint32_t clamp_u32(uint32_t x, uint32_t lo, uint32_t hi){
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

int stepper_home_read(const stepper_motor *m, int *raw_out, int *active_out){
    if (!m || !g.home) return -1;
    int v = gpiod_line_get_value(g.home);
    if (v < 0) return -2;

    if (raw_out) *raw_out = v;
    if (active_out) *active_out = (v == (int)m->home_active_level) ? 1 : 0;
    return 0;
}

static int home_is_active(const stepper_motor *m){
    int active = 0;
    (void)stepper_home_read(m, NULL, &active);
    return active;
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

    int en_idle = (m->en_active_level ? 0 : 1);
    if (gpiod_line_request_output(g.en, "stp_en", en_idle) < 0) return -6;

    g.home = gpiod_chip_get_line(g.chip, m->home_pin);
    if (!g.home) return -7;
    if (gpiod_line_request_input(g.home, "stp_home") < 0) return -8;

    m->cur_pos_stp       = 0;
    m->cur_speed_sps     = 0;
    m->cur_speed_fp      = 0;

    m->target_pos_stp    = 0;
    m->target_speed_sps  = 0;
    m->target_acc_sps2   = 0;

    m->last_speed_us     = 0;
    m->enabled_at_us     = 0;
    m->need_dir_setup    = 0;

    m->step_level        = 0;
    m->next_edge_us      = 0;
    gpiod_line_set_value(g.step, 0);

    m->homed = 0;
    m->state = STP_READY;
    g.m = m;
    return 0;
}

int stepper_enable(stepper_motor *m){
    if (!m || g.m != m || !g.en) return -1;

    gpiod_line_set_value(g.en, m->en_active_level ? 1 : 0);
    m->enabled_at_us = now_us_local();

    m->state = STP_ENABLED;
    return 0;
}

int stepper_disable(stepper_motor *m){
    if (!m || g.m != m || !g.en) return -1;
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
    if (!m || g.m != m) return -1;
    if (speed_sps == 0) return -2;

    m->target_pos_stp    = abs_stp;
    m->target_speed_sps  = speed_sps;
    m->target_acc_sps2   = acc_sps2;

    m->cur_speed_fp      = 0;
    m->cur_speed_sps     = 0;
    m->last_speed_us     = 0;

    m->step_level        = 0;
    m->next_edge_us      = 0;
    gpiod_line_set_value(g.step, 0);

    int32_t delta = m->target_pos_stp - m->cur_pos_stp;
    int dir = (delta >= 0) ? 1 : 0;
    if (m->dir_invert) dir ^= 1;
    gpiod_line_set_value(g.dir, dir);
    m->need_dir_setup = 1;

    m->state = STP_MOVING;
    return 0;
}

int stepper_start_move_rel(stepper_motor *m, int32_t delta_stp, uint32_t speed_sps, uint32_t acc_sps2){
    if (!m) return -1;
    return stepper_start_move_abs(m, m->cur_pos_stp + delta_stp, speed_sps, acc_sps2);
}

int stepper_start_homing(stepper_motor *m, uint32_t speed_sps, uint32_t acc_sps2, int8_t dir){
    if (!m || g.m != m) return -1;
    if (speed_sps == 0) return -2;
    if (dir != 1 && dir != -1) return -3;

    m->target_speed_sps = speed_sps;
    m->target_acc_sps2  = acc_sps2;
    m->cur_speed_fp     = 0;
    m->cur_speed_sps    = 0;
    m->last_speed_us    = 0;

    m->step_level   = 0;
    m->next_edge_us = 0;
    gpiod_line_set_value(g.step, 0);

    int out_dir = (dir > 0) ? 1 : 0;
    if (m->dir_invert) out_dir ^= 1;
    gpiod_line_set_value(g.dir, out_dir);
    m->need_dir_setup = 1;

    m->homed = 0;
    m->state = STP_HOMING;
    return 0;
}

void stepper_update(stepper_motor *m, uint32_t now_us){
    if (!m || g.m != m) return;
    if (m->state != STP_MOVING && m->state != STP_HOMING) return;

    if (m->enabled_at_us != 0) {
        if ((uint32_t)(now_us - m->enabled_at_us) < ENABLE_SETTLE_US) return;
    }

    int32_t delta = m->target_pos_stp - m->cur_pos_stp;

    if (m->state == STP_MOVING) {
        if (delta == 0) { m->state = STP_ENABLED; return; }
    } else { // STP_HOMING
        if (home_is_active(m)) {
            gpiod_line_set_value(g.step, 0);
            m->step_level = 0;
            m->next_edge_us = 0;
            m->cur_speed_fp = 0;
            m->cur_speed_sps = 0;
            m->state = STP_ENABLED;
            m->homed = 1;
            return;
        }
    }

    if (m->need_dir_setup) {
        m->need_dir_setup = 0;
        m->last_speed_us  = now_us;
        m->next_edge_us   = now_us + DIR_SETUP_US;
        m->step_level     = 0;
        gpiod_line_set_value(g.step, 0);
        return;
    }

    if (m->last_speed_us == 0) m->last_speed_us = now_us;
    uint32_t dt_us = (uint32_t)(now_us - m->last_speed_us);
    m->last_speed_us = now_us;

    uint32_t acc = m->target_acc_sps2;
    if (acc == 0) {
        m->cur_speed_fp  = (uint64_t)m->target_speed_sps * 1000000ull;
    } else {
        m->cur_speed_fp += (uint64_t)acc * (uint64_t)dt_us;
        uint64_t max_fp = (uint64_t)m->target_speed_sps * 1000000ull;
        if (m->cur_speed_fp > max_fp) m->cur_speed_fp = max_fp;
    }

    m->cur_speed_sps = (uint32_t)(m->cur_speed_fp / 1000000ull);
    uint32_t sps = m->cur_speed_sps;
    if (sps < 1u) sps = 1u;
    sps = clamp_u32(sps, 1u, MAX_SPS);

    uint32_t period_us = 1000000u / sps;

    uint32_t pw = m->pulse_width_us;
    if (pw < 3u) pw = 3u;
    if (pw >= period_us) pw = period_us / 2u;
    if (pw == 0u) pw = 1u;

    if (m->next_edge_us == 0) {
        m->next_edge_us = now_us + period_us;
        return;
    }

    if ((int32_t)(now_us - m->next_edge_us) < 0) return;

    if (m->step_level == 0) {
        gpiod_line_set_value(g.step, 1);
        m->step_level = 1;
        m->next_edge_us = now_us + pw;
    } else {
        gpiod_line_set_value(g.step, 0);
        m->step_level = 0;

        if (m->state == STP_MOVING) {
            delta = m->target_pos_stp - m->cur_pos_stp;
            if (delta != 0) {
                int step_dir = (delta > 0) ? +1 : -1;
                m->cur_pos_stp += step_dir;
            }
        } else {
            // homing: position is just "software tracking"
            int dir_out = gpiod_line_get_value(g.dir);
            int phys_dir = dir_out ? +1 : -1;
            if (m->dir_invert) phys_dir = -phys_dir;
            m->cur_pos_stp += phys_dir;
        }

        uint32_t low_us = (period_us > pw) ? (period_us - pw) : 1u;
        m->next_edge_us = now_us + low_us;
        m->next_edge_us = now_us + low_us;
    }
}

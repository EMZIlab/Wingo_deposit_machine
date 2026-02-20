// File: src/core/core.c
#include "core.h"

#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdatomic.h>
#include <sys/stat.h>

#include "stepper_driver.h"
#include "stepper_thread.h"
#include "hx711_driver.h"
#include "hx711_thread.h"
#include "shared.h"
#include "config.h"

static void nsleep_ms(long ms){
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    (void)nanosleep(&ts, 0);
}

static int file_is_empty(const char *path){
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return (st.st_size == 0);
}

static void append_csv(const char *path, double avg_kg, const app_config_t *cfg){
    FILE *f = fopen(path, "a");
    if (!f) {
        perror("csv fopen");
        return;
    }

    if (file_is_empty(path)) {
        fprintf(f, "settle time, sample period, avg_kg\n");
    }

    fprintf(f, "%i, %i, %.3f\n", cfg->settle_ms, cfg->sample_period_ms, avg_kg);
    fclose(f);
}

static void weigh_and_log(const volatile sig_atomic_t *running, const app_config_t *cfg) {
    double sum = 0.0;
    uint32_t n = (cfg->sample_count == 0) ? 1u : cfg->sample_count;
    for (uint32_t i = 0; i < n && *running; i++) {
        float kg = atomic_load(&g_scale_kg);
        sum += (double)kg;
        nsleep_ms((long)cfg->sample_period_ms);
    }
    double avg = sum / (double)n;
    if (!*running) return;
    // 6) append to csv
    append_csv(cfg->csv_path, avg, cfg);
    fprintf(stderr, "logged avg=%.6f kg to %s\n", avg, cfg->csv_path);
}

static void weight_treshold(const volatile sig_atomic_t *running, const app_config_t *cfg, stepper_motor *m1) {
    if (!running || !cfg || !m1 || !*running) return;

    float w0 = atomic_load(&g_scale_kg);
    if (w0 <= cfg->trig_treshold) return;

    nsleep_ms((long)cfg->settle_ms);
    if (!*running) return;

    float w1 = atomic_load(&g_scale_kg);
    if (w1 < cfg->trig_treshold) return;

    float dw = w1 - w0;
    if (dw < 0.005f) dw = -dw;
    if (dw > 0.005f) return;

    weigh_and_log(running, cfg);

    (void)stepper_start_move_abs(m1, cfg->move_stp, cfg->move_speed_sps, cfg->move_acc_sps2);
    while (*running && m1->state == STP_MOVING) nsleep_ms(10);
    if (!*running) return;

    (void)stepper_start_move_abs(m1, 0, cfg->move_speed_sps, cfg->move_acc_sps2);
    while (*running && m1->state == STP_MOVING) nsleep_ms(10);
}

void start_core(const volatile sig_atomic_t* running){
    app_config_t cfg;
    config_set_defaults(&cfg);

    int rc = config_load_file(&cfg, "/home/pi5/dev/Wingo_deposit_machine/config.txt");
    if (rc == -1) {
        fprintf(stderr, "config: config.txt not found, using defaults\n");
    } else if (rc == -2) {
        fprintf(stderr, "config: config.txt had parse errors, using best-effort values\n");
    } else {
        fprintf(stderr, "config: loaded config.txt\n");
    }

    hx711_t scale = {
        .gpiochip = "/dev/gpiochip4",
        .sck_line = 6,
        .dout_line = 5,
        .tare_offset_cts = cfg.hx_tare_offset_cts,
        .counts_per_kg   = cfg.hx_counts_per_kg,
    };

    stepper_motor m1 = {
        .gpiochip = "/dev/gpiochip4",
        .stp_per_rev = 8000u,
        .pulse_width_us = 10u,

        .pul_pin = 24,
        .dir_pin = 23,
        .enable_pin = 17,

        .home_pin = 27,
        .home_active_level = 0,

        .dir_invert = 0,
        .en_active_level = 0,
    };

    if (stepper_init(&m1) < 0) return;
    if (stepper_enable(&m1) < 0) return;

    if (hx711_init(&scale) == 0) {
        (void)hx711_thread_start(running, &scale);
    }

    (void)stepper_thread_start(running, &m1, 80, 2);

    int raw = -1, active = 0;
    int rr = stepper_home_read(&m1, &raw, &active); // modify to give only active not raw value.
    if (active && rr == 0) {
        const int32_t backoff_steps = 500;
        int32_t delta = (int32_t)(-cfg.home_dir) * backoff_steps;

        fprintf(stderr, "[HOME] switch already active -> backoff %ld steps (delta=%ld)\n",
                (long)backoff_steps, (long)delta);

        (void)stepper_start_move_rel(&m1, delta, cfg.home_speed_sps, cfg.home_acc_sps2);

        while (*running && m1.state == STP_MOVING) {
            nsleep_ms(20);
        }
        if (!*running) return;
    }

    fflush(stderr);

    m1.homed = 0;
    (void)stepper_start_homing(&m1, cfg.home_speed_sps, cfg.home_acc_sps2, (int8_t)cfg.home_dir);

    while (*running && m1.homed == 0) {
        nsleep_ms(20);
    }
    if (!*running) return;

    stepper_set_pos(&m1, cfg.home_offset_steps);
    (void)stepper_start_move_abs(&m1, 0, cfg.home_speed_sps, cfg.home_acc_sps2);

    while (*running && m1.state == STP_MOVING) {
        nsleep_ms(20);
    }
    if (!*running) return;

    nsleep_ms((long)cfg.settle_ms);

    while (*running) {
        float weight = atomic_load(&g_scale_kg);
        weight_treshold(running, &cfg, &m1);
        int raw_scale = atomic_load(&scale_raw_value);
        // float deg = stepper_get_pos_deg(&m1);
        printf("scale = %.3f kg | raw_scale = %i\n",
               weight, raw_scale);
        fflush(stdout);
        nsleep_ms(200);
    }

    hx711_close(&scale);
    (void)stepper_disable(&m1);
}

#include "core.h"
#include <stdio.h>
#include <gpiod.h>
#include <signal.h>
#include <time.h>
#include "hx711_driver.h"
#include "hx711_thread.h"
#include "stepper_driver.h"
#include "shared.h"
#include <stdatomic.h>



// int setup() {
    
// }

static uint32_t now_us(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = (uint64_t)(int64_t)ts.tv_sec * 1000000u
                + (uint64_t)(int64_t)ts.tv_nsec / 1000u;
    return (uint32_t)us;
}

static void nsleep(long ns){
    struct timespec ts = { .tv_sec = 0, .tv_nsec = ns };
    nanosleep(&ts, 0);
}

void start_core(const volatile sig_atomic_t* running) {
    // setup();

    hx711_t scale = {
        .gpiochip = "/dev/gpiochip4",
        .sck_line = 6,
        .dout_line = 5,
        .tare_offset_cts = 1748000,     // set yours
        .counts_per_kg   = 951010.0f,   // your calibration
    };

        stepper_motor m1 = {
        .gpiochip = "/dev/gpiochip4",
        .stp_per_rev = 200u * 4u,
        .pulse_width_us = 50u,
        .pul_pin = 24,
        .dir_pin = 23,
        .enable_pin = 17,
        .home_pin = 22,
        .home_active_level = 0,
        .dir_invert = 0,
        .en_active_level = 0,
    };

    if (stepper_init(&m1) < 0) return;
    if (stepper_enable(&m1) < 0) return;

    (void)stepper_start_move_rel(&m1, 8000, 1000u, 500u);

    if (hx711_init(&scale) == 0) {
        hx711_thread_start(running, &scale);
    }

    while(*running) {
        stepper_update(&m1, now_us());
        nsleep(1000000L); // 1 ms loop
    }

    hx711_close(&scale);
    stepper_disable(&m1);
}
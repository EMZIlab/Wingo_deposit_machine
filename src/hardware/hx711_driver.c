#include "hx711_driver.h"

#include <gpiod.h>
#include <time.h>

static void nsleep(long ns){
    struct timespec ts = { .tv_sec = 0, .tv_nsec = ns };
    nanosleep(&ts, 0);
}

static uint32_t now_ms(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)(int64_t)ts.tv_sec * 1000u
                + (uint64_t)(int64_t)ts.tv_nsec / 1000000u;
    return (uint32_t)ms;
}

static int wait_ready(struct gpiod_line *dout, uint32_t timeout_ms){
    uint32_t t0 = now_ms();
    while (gpiod_line_get_value(dout) == 1) {
        if ((uint32_t)(now_ms() - t0) > timeout_ms) return -1;
        nsleep(500L * 1000L);
    }
    return 0;
}

int hx711_init(hx711_t *h){
    struct gpiod_chip *chip = gpiod_chip_open(h->gpiochip);
    if (!chip) return -1;

    struct gpiod_line *sck  = gpiod_chip_get_line(chip, h->sck_line);
    struct gpiod_line *dout = gpiod_chip_get_line(chip, h->dout_line);
    if (!sck || !dout) { gpiod_chip_close(chip); return -2; }

    if (gpiod_line_request_output(sck, "hx711_sck", 0) < 0) { gpiod_chip_close(chip); return -3; }
    if (gpiod_line_request_input(dout, "hx711_dout") < 0)   { gpiod_line_release(sck); gpiod_chip_close(chip); return -4; }

    gpiod_line_set_value(sck, 0);

    h->chip = chip;
    h->sck  = sck;
    h->dout = dout;
    return 0;
}

void hx711_close(hx711_t *h){
    if (!h) return;
    if (h->dout) gpiod_line_release((struct gpiod_line*)h->dout);
    if (h->sck)  gpiod_line_release((struct gpiod_line*)h->sck);
    if (h->chip) gpiod_chip_close((struct gpiod_chip*)h->chip);
    h->chip = h->sck = h->dout = 0;
}

int hx711_read_raw(hx711_t *h, int32_t *raw_out){
    struct gpiod_line *sck  = (struct gpiod_line*)h->sck;
    struct gpiod_line *dout = (struct gpiod_line*)h->dout;

    if (!sck || !dout || !raw_out) return -1;
    if (wait_ready(dout, 2000) < 0) return -2;

    uint32_t raw = 0;

    gpiod_line_set_value(sck, 0);

    for (int i = 0; i < 24; i++) {
        gpiod_line_set_value(sck, 1);
        gpiod_line_set_value(sck, 0);
        raw = (raw << 1) | (uint32_t)(gpiod_line_get_value(dout) & 1);
    }

    gpiod_line_set_value(sck, 1);
    gpiod_line_set_value(sck, 0);

    if (raw & 0x800000u) raw |= 0xFF000000u;
    *raw_out = (int32_t)raw;
    return 0;
}

float hx711_raw_to_kg(const hx711_t *h, int32_t raw){
    return (float)(raw - h->tare_offset_cts) / h->counts_per_kg;
}

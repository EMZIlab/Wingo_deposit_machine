#pragma once
#include <stdint.h>

typedef struct hx711 {
    const char *gpiochip;
    uint8_t sck_line;
    uint8_t dout_line;
    int32_t tare_offset_cts;
    float counts_per_kg;
    void *chip;
    void *sck;
    void *dout;
} hx711_t;

int  hx711_init(hx711_t *h);
void hx711_close(hx711_t *h);

int  hx711_read_raw(hx711_t *h, int32_t *raw);
float hx711_raw_to_kg(const hx711_t *h, int32_t raw);

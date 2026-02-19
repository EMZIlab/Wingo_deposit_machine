// File: src/config/config.h
#pragma once
#include <stdint.h>

typedef struct app_config {
    // ---- HX711 calibration ----
    int32_t  hx_tare_offset_cts;
    float    hx_counts_per_kg;

    // ---- Move target ----
    int32_t move_stp;
    uint32_t move_speed_sps;
    uint32_t move_acc_sps2;

    // ---- Homing ----
    int32_t  home_offset_steps;     // after switch hit: set pos = home_offset_steps
    int32_t  home_dir;              // +1 or -1
    uint32_t home_speed_sps;
    uint32_t home_acc_sps2;

    // Weight trigger
    float trig_treshold;

    // ---- Sampling ----
    uint32_t settle_ms;             // wait after reaching 0
    uint32_t sample_count;          // N samples
    uint32_t sample_period_ms;      // delay between samples

    // ---- Logging ----
    char     csv_path[256];
} app_config_t;

// Fill cfg with defaults
void config_set_defaults(app_config_t *cfg);

// Load config file, overriding defaults.
// Returns: 0 if loaded OK, -1 if file missing/unreadable (defaults remain), -2 parse error (still best-effort)
int  config_load_file(app_config_t *cfg, const char *path);

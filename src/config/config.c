// File: src/config/config.c
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

static char* ltrim(char *s){
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void rtrim_inplace(char *s){
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1])) {
        s[n-1] = '\0';
        n--;
    }
}

static int streq(const char *a, const char *b){
    return strcmp(a, b) == 0;
}

void config_set_defaults(app_config_t *c){
    if (!c) return;
    memset(c, 0, sizeof(*c));

    // HX711 calibration defaults
    c->hx_tare_offset_cts = 1748000;
    c->hx_counts_per_kg   = 951010.0f;

    // Move defaults
    c->move_speed_sps = 10000u;
    c->move_acc_sps2  = 10000u;

    // Homing defaults
    c->home_offset_steps = 0;     // set to your sensor offset in steps
    c->home_dir          = -1;    // direction to move to hit switch (+1 / -1)
    c->home_speed_sps    = 3000u;
    c->home_acc_sps2     = 8000u;

    // Weight treshold default
    c->trig_treshold = 0.030f;

    // Sampling defaults
    c->settle_ms        = 1000u;
    c->sample_count     = 20u;
    c->sample_period_ms = 50u;

    // CSV default path
    strncpy(c->csv_path, "/home/pi5/dev/Wingo_deposit_machine/scale_log.csv", sizeof(c->csv_path)-1);
    c->csv_path[sizeof(c->csv_path)-1] = '\0';
}

static int parse_u32(const char *s, uint32_t *out){
    if (!s || !out) return -1;
    errno = 0;
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);
    if (errno != 0 || end == s) return -1;
    *out = (uint32_t)v;
    return 0;
}

static int parse_i32(const char *s, int32_t *out){
    if (!s || !out) return -1;
    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 0);
    if (errno != 0 || end == s) return -1;
    *out = (int32_t)v;
    return 0;
}

static int parse_f32(const char *s, float *out){
    if (!s || !out) return -1;
    errno = 0;
    char *end = NULL;
    float v = strtof(s, &end);
    if (errno != 0 || end == s) return -1;
    *out = v;
    return 0;
}

static int apply_kv(app_config_t *c, const char *k, const char *v){
    // HX711 calibration
    if (streq(k, "hx.tare_offset_cts")) return parse_i32(v, &c->hx_tare_offset_cts);
    if (streq(k, "hx.counts_per_kg"))   return parse_f32(v, &c->hx_counts_per_kg);

    // Move
    if (streq(k, "move.speed_sps")) return parse_u32(v, &c->move_speed_sps);
    if (streq(k, "move.acc_sps2"))  return parse_u32(v, &c->move_acc_sps2);

    // Homing
    if (streq(k, "home.offset_steps")) return parse_i32(v, &c->home_offset_steps);
    if (streq(k, "home.dir"))          return parse_i32(v, &c->home_dir);
    if (streq(k, "home.speed_sps"))    return parse_u32(v, &c->home_speed_sps);
    if (streq(k, "home.acc_sps2"))     return parse_u32(v, &c->home_acc_sps2);

    // Weight trigger
    if (streq(k, "trigger.treshold"))   return parse_f32(v, &c->trig_treshold);

    // Sampling
    if (streq(k, "sample.settle_ms"))   return parse_u32(v, &c->settle_ms);
    if (streq(k, "sample.count"))       return parse_u32(v, &c->sample_count);
    if (streq(k, "sample.period_ms"))   return parse_u32(v, &c->sample_period_ms);

    // Logging
    if (streq(k, "log.csv_path")) {
        strncpy(c->csv_path, v, sizeof(c->csv_path)-1);
        c->csv_path[sizeof(c->csv_path)-1] = '\0';
        return 0;
    }

    // Unknown key: ignore
    return 0;
}

int config_load_file(app_config_t *c, const char *path){
    if (!c || !path) return -2;

    FILE *f = fopen(path, "r");
    if (!f) return -1; // missing/unreadable is not fatal: keep defaults

    char line[512];
    int any_parse_error = 0;

    while (fgets(line, sizeof(line), f)) {
        char *p = line;

        // BOM
        if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p += 3;
        }

        // comments (# or ;)
        char *hash = strchr(p, '#');
        char *semi = strchr(p, ';');
        char *cut = NULL;
        if (hash && semi) cut = (hash < semi) ? hash : semi;
        else cut = hash ? hash : semi;
        if (cut) *cut = '\0';

        p = ltrim(p);
        rtrim_inplace(p);
        if (*p == '\0') continue;

        char *eq = strchr(p, '=');
        if (!eq) { any_parse_error = 1; continue; }

        *eq = '\0';
        char *k = ltrim(p);
        rtrim_inplace(k);

        char *v = ltrim(eq + 1);
        rtrim_inplace(v);

        if (*k == '\0') { any_parse_error = 1; continue; }

        if (apply_kv(c, k, v) != 0) {
            any_parse_error = 1;
        }
    }

    fclose(f);
    return any_parse_error ? -2 : 0;
}

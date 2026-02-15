#include "core.h"
#include <stdio.h>
#include <gpiod.h>
#include <signal.h>
// #include <unistd.h>
#include <time.h>



// int setup() {
    
// }

void start_core(const volatile sig_atomic_t* running) {
    // setup();
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000 }; // 100 us
    const unsigned outs[] = {16,13,12,24,23,17};
    const unsigned ins[]  = {27,22};
    const int n_out = (int)(sizeof(outs)/sizeof(outs[0]));
    const int n_in  = (int)(sizeof(ins)/sizeof(ins[0]));

    struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip4");
    if (!chip) { perror("gpiod_chip_open"); return; }

    struct gpiod_line_bulk ob, ib;
    gpiod_line_bulk_init(&ob);
    gpiod_line_bulk_init(&ib);

    for (int i=0;i<n_out;i++) gpiod_line_bulk_add(&ob, gpiod_chip_get_line(chip, outs[i]));
    for (int i=0;i<n_in;i++)  gpiod_line_bulk_add(&ib, gpiod_chip_get_line(chip, ins[i]));

    int init_vals[6] = {0,0,0,0,0,0};
    if (gpiod_line_request_bulk_output(&ob, "out", init_vals) < 0) { perror("req out"); return; }
    if (gpiod_line_request_bulk_input(&ib, "in") < 0)            { perror("req in");  return; }

    int out_state = 0;

    while(*running) {
        int in_vals[2];
        if (gpiod_line_get_value_bulk(&ib, in_vals) < 0) { perror("read in"); break; }

        int limit = (in_vals[0]==0) || (in_vals[1]==0);     // active-low limit switches

        if (limit) out_state = 0; else out_state ^= 1;      // toggle when no limit

        int out_vals[6];
        for (int i=0;i<n_out;i++) out_vals[i] = out_state;
        if (gpiod_line_set_value_bulk(&ob, out_vals) < 0) { perror("write out"); break; }

        printf("IN27=%d IN22=%d  limit=%d  OUT=%d\n", in_vals[0], in_vals[1], limit, out_state);
        fflush(stdout);

        
        nanosleep(&ts, NULL); // 5 Hz toggle/print

    }

    // safe low
    int out_low[6] = {0,0,0,0,0,0};
    gpiod_line_set_value_bulk(&ob, out_low);

    gpiod_line_release_bulk(&ib);
    gpiod_line_release_bulk(&ob);
    gpiod_chip_close(chip);
    return;
}
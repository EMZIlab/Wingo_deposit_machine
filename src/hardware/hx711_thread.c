#include "hx711_thread.h"
#include "shared.h"

#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

typedef struct {
    const volatile sig_atomic_t *running;
    hx711_t *dev;
} hx711_thr_args_t;

static void nsleep(long ns){
    struct timespec ts = { .tv_sec = 0, .tv_nsec = ns };
    nanosleep(&ts, 0);
}

static void* hx711_thread_fn(void *p){
    hx711_thr_args_t *a = (hx711_thr_args_t*)p;

    while (*(a->running)) {
        int32_t raw;
        if (hx711_read_raw(a->dev, &raw) == 0) {
            float kg = hx711_raw_to_kg(a->dev, raw);
            atomic_store(&scale_raw_value, raw);
            atomic_store(&g_scale_kg, kg);
        }
        nsleep(50L * 1000L * 1000L); // 50 ms
    }
    return 0;
}

int hx711_thread_start(const volatile sig_atomic_t *running, hx711_t *dev){
    static pthread_t th;
    static hx711_thr_args_t args;

    args.running = running;
    args.dev = dev;

    return pthread_create(&th, 0, hx711_thread_fn, &args);
}

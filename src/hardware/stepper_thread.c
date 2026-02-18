#include "stepper_thread.h"

#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#ifdef __linux__
#include <sched.h>
#include <sys/mman.h>
#endif

static uint32_t now_us(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = (uint64_t)(int64_t)ts.tv_sec * 1000000u
                + (uint64_t)(int64_t)ts.tv_nsec / 1000u;
    return (uint32_t)us;
}

static void ts_add_ns(struct timespec *t, long ns){
    t->tv_nsec += ns;
    while (t->tv_nsec >= 1000000000L) { t->tv_nsec -= 1000000000L; t->tv_sec++; }
    while (t->tv_nsec < 0)            { t->tv_nsec += 1000000000L; t->tv_sec--; }
}

typedef struct {
    const volatile sig_atomic_t *running;
    stepper_motor *m;
    int rt_priority;
    int cpu_affinity;
} stp_thr_args_t;

static void apply_rt_settings(int rt_priority, int cpu_affinity){
#ifdef __linux__
    // Prevent paging jitter
    (void)mlockall(MCL_CURRENT | MCL_FUTURE);

    // Optional CPU pinning
    if (cpu_affinity >= 0) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET((unsigned)cpu_affinity, &set);
        (void)pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    }

    // Optional SCHED_FIFO
    if (rt_priority > 0) {
        struct sched_param sp;
        memset(&sp, 0, sizeof(sp));
        sp.sched_priority = rt_priority;
        int rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
        if (rc != 0) {
            // If you didn't run with CAP_SYS_NICE / as root, this will fail.
            // We continue anyway.
            fprintf(stderr, "stepper_thread: pthread_setschedparam FIFO failed: %s\n", strerror(rc));
        }
    }
#else
    (void)rt_priority; (void)cpu_affinity;
#endif
}

static void* stepper_thread_fn(void *p){
    stp_thr_args_t *a = (stp_thr_args_t*)p;

    apply_rt_settings(a->rt_priority, a->cpu_affinity);

    // 50 us tick is a good starting point (20 kHz loop),
    // your step pulses are generated inside stepper_update().
    const long tick_ns = 50L * 1000L;

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (*(a->running)) {
        // run scheduler
        stepper_update(a->m, now_us());

        // absolute sleep (prevents drift)
        ts_add_ns(&next, tick_ns);
        int rc;
        do {
            rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        } while (rc == EINTR);
    }

    return NULL;
}

int stepper_thread_start(const volatile sig_atomic_t *running,
                         stepper_motor *m,
                         int rt_priority,
                         int cpu_affinity)
{
    static pthread_t th;
    static stp_thr_args_t args;

    args.running = running;
    args.m = m;
    args.rt_priority = rt_priority;
    args.cpu_affinity = cpu_affinity;

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    // Larger stack not needed, but keep default.
    int rc = pthread_create(&th, &attr, stepper_thread_fn, &args);
    pthread_attr_destroy(&attr);
    return rc;
}

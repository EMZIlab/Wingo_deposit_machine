#pragma once
#include <signal.h>
#include "stepper_driver.h"

int stepper_thread_start(const volatile sig_atomic_t *running,
                         stepper_motor *m,
                         int rt_priority,     // e.g. 80 (0 disables RT policy)
                         int cpu_affinity);   // e.g. 2 (or -1 = no pin)

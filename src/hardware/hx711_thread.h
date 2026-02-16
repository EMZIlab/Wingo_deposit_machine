#pragma once
#include <signal.h>
#include "hx711_driver.h"

int hx711_thread_start(const volatile sig_atomic_t *running, hx711_t *dev);

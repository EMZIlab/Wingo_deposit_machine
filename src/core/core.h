#pragma once

#include <signal.h>
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>


static volatile int run = 1;
// static void stop(int s){ (void)s; run = 0; }

void start_core(const volatile sig_atomic_t* running_flag);
#include "core.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>

static volatile sig_atomic_t running = 1;
static void on_sigint(int sigint) {
  ( void )sigint;
  running = 0;
}


int main(void) {
  signal(SIGINT, on_sigint);
  start_core(&running);
  return 0;
}
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handle_sigint(
    int sig
) {
    printf("SHUTDOWN SIGNAL: %d\n", sig);
    _exit(0);
}
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>

#include "workers_clients.h"

int nbo, nbc;
int pipes[NBO_MAX][2];  // pipes between workers
sigset_t client_mask;   // sisgsuspend mask

void terminate(int errcode, const char *msg, ...) {
    va_list specifiers;
    va_start(specifiers, msg);
    vfprintf(stderr, msg, specifiers);
    fprintf(stderr, "\n%s", strerror(errno));
    va_end(specifiers);
    exit(errcode);
}

// what a client has to do when receiving SIGUSR1
void cmd_rcv_handler(int sig) {
    // just catch the signal
}

int main(int argc, char **argv) {
    puts("----<> program starting <>----");


    struct sigaction cmd_rcv_act;
    cmd_rcv_act.sa_flags = 0;
    if (sigemptyset(&cmd_rcv_act.sa_mask) < 0) 
        terminate(errno, "sigemptyset for sigaction");
    cmd_rcv_act.sa_handler = cmd_rcv_handler;
    if (sigemptyset(&client_mask) < 0)
        terminate(errno, "sigemptyset for sigsuspend");
    
    // should make sure SIGUSR1 is not blocked for clients

    if (sigaction(SIGUSR1, &cmd_rcv_act, NULL) < 0) 
        terminate(errno, "sigaction");

    if (argc != 3) {
        terminate(EXIT_FAILURE, "usage: %s <nb of workers> <nb of clients>", argv[0]);
    }
    
    nbo = (atoi(argv[1]) > NBO_MAX) ? NBO_MAX : atoi(argv[1]);
    nbc = (atoi(argv[2]) > NBC_MAX) ? NBC_MAX : atoi(argv[2]);

    // creating workers pipes
    for (int i = 0; i < nbo; i++) {
        if (pipe(pipes[i]) < 0)
            terminate(errno, "creating worker pipe #%d", i);
    }

    for (int i = 0; i < nbc; i++) {
        switch (fork()) {
            case -1:
                terminate(errno, "fork client #%d", i);
            case 0:
                client(i);
            default:;
        }
    }

    for (int i = 0; i < nbo; i++) {
        switch (fork()) {
            case -1:
                terminate(errno, "fork worker #%d", i);
            case 0:
                worker(i);
            default:;
        }
    }

    close_client_pipes();
    close(pipes[0][1]);

    // wait for all workers/clients to finish
    while (wait(NULL) != -1)
        ;

    puts("----<> program terminating <>----");

    return EXIT_SUCCESS;
}
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>

#include "workers_clients.h"

extern sigset_t client_mask;
extern int nbo, nbc;
extern int pipes[NBO_MAX][2];  // pipes between workers


extern void terminate(int errcode, const char *msg, ...);

// close unused pipes
void close_pipes(int pc_num) {
    if (pc_num < nbo - 1) {
        for (int i = 0; i < nbo; i++) {
            if (i != pc_num && i != pc_num + 1) {
                close(pipes[i][1]);
                close(pipes[i][0]);
            } else {
                close(pipes[pc_num][1]);
                close(pipes[pc_num + 1][0]);
            }
        }
    } else {
        for (int i = 0; i < nbo - 1; i++) {
            close(pipes[i][1]);
            close(pipes[i][0]);
        }
        close(pipes[nbo - 1][1]);
    }
}

void close_client_pipes() {
    for (int i = 1; i < nbo; i++) {
        close(pipes[i][1]);
        close(pipes[i][0]);
    }
    close(pipes[0][0]);
}

void print_n_modify(int pc_num, data *command) {
    char buffer[NBO_MAX * 50];
    char aux_buf[50];
    printf("Worker #%d: received command from client %d (%d) -- ind=%d\n", 
            pc_num, command -> pc_num, command -> pid, ++command -> ind);
    
    sprintf(buffer, "   << W%d: ", pc_num);
    for (int i = 0; i < nbo; i++) {
        sprintf(aux_buf, "infos[%d]=%d ", i, command -> infos[i]);
        strcat(buffer, aux_buf);
    }
    strcat(buffer, ">>");
    printf("%s\n%c", buffer, (command -> pc_num < nbc - 1) ? ' ' : '\n');

    command -> infos[pc_num] = pc_num;
}

void worker(int pc_num) {
    close_pipes(pc_num);
    int status;
    data command;
    while (status = read(pipes[pc_num][0], &command, sizeof(data)) != 0)
        if (status < 0)
            terminate(errno, "worker %d reading in pipe #%d", pc_num, pc_num);
        else {
            print_n_modify(pc_num, &command);
            // printf("w %d: rcv cmd client %d (%d) -- ind=%d\n", pc_num, command.pc_num, command.pid, ++command.ind);
            if (pc_num < nbo - 1) {
                if ((status = write(pipes[pc_num + 1][1], &command, sizeof(data))) < 0)
                    terminate(errno, "worker %d writing in worker pipe #%d", pc_num, pc_num + 1);
            } else {
                // last worker => send SIGURS1 to the client
                if (kill(command.pid, SIGUSR1) < 0)
                    terminate(errno, "last worker sending SIGURS1 to %d", command.pid);
            }
        }
    close(pipes[pc_num][0]);
    if (pc_num < nbo - 1) 
        close(pipes[pc_num + 1][1]);
    exit(EXIT_SUCCESS);
}


void client(int pc_num) {
    close_client_pipes();
    pid_t cpid = getpid();
    // initializing the command
    data command = {.pid=cpid, .pc_num=pc_num, .ind=0};
    for (int i = 0; i < nbo; i++)   // useless to loop until i = NBO_MAX
        command.infos[i] = cpid;

    printf("\tclient %d (%d): command sent, waiting...\n", pc_num, cpid);

    if (write(pipes[0][1], &command, sizeof(data)) < 0) 
        terminate(errno, "client %d writing in pipe #%d", pc_num, 0);
    sigsuspend(&client_mask);     // wait for SIGUSR1
    // SIGURS1 received => command received
    printf("\tclient %d (%d): command received\n\n", pc_num, cpid);

    close(pipes[0][1]);
    exit(EXIT_SUCCESS);
}
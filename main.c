#include <unistd.h>
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


#define NBO_MAX 5
#define NBC_MAX 5

int nbo, nbc;
int pipe_cw[2];             // pipe client -> worker 0
int pipes[NBO_MAX - 1][2];  // pipes between workers

sigset_t client_mask;

// client command request
typedef struct {
    pid_t pid;      // pid
    int pc_num;     // process number
    int ind;        // indicator
} data;


void terminate(int errcode, const char *msg, ...) {
    va_list specifiers;
    va_start(specifiers, msg);
    vfprintf(stderr, msg, specifiers);
    fprintf(stderr, "\n%s", strerror(errno));
    va_end(specifiers);
    exit(errcode);
}

// close unused pipes by worker pc_num
void close_wpipes(int pc_num) {
    if (pc_num > 0) {
        close(pipe_cw[0]);
        close(pipe_cw[1]);
        for (int i = 0; i < nbo - 1; i++) {
            if (i != pc_num && i != pc_num - 1) {
                close(pipes[i][1]);
                close(pipes[i][0]);
            }
        }
    } else {
        for (int i = 1; i < nbo - 1; i++) {
            close(pipes[i][1]);
            close(pipes[i][0]);
        }
    }
}

// close unused pipes by client pc_num
void close_cpipes() {
    for (int i = 0; i < nbo - 1; i++) {
        close(pipes[i][1]);
        close(pipes[i][0]);
    }
}

void worker0_task(int pc_num) {
    close(pipe_cw[1]);
    int status;
    data command;
    while (status = read(pipe_cw[0], &command, sizeof(data)) != 0)
        if (status < 0)
            terminate(errno, "worker %d reading in cw pipe", pc_num);
        else {
            printf("Worker #%d: received command from client %d (%d) -- ind=%d\n", 
                    pc_num, command.pc_num, command.pid, ++command.ind);
            if ((status = write(pipes[pc_num][1], &command, sizeof(data))) < 0)
                terminate(errno, "worker %d writing in worker pipe %d", pc_num, 0);
        }
    close(pipes[pc_num][1]);
    exit(EXIT_SUCCESS);
}

void worker_last_task(int pc_num) {
    close(pipes[pc_num - 1][1]);
    int status;
    data command;
    while (status = read(pipes[pc_num - 1][0], &command, sizeof(data)) != 0)
        if (status < 0)
            terminate(errno, "worker %d reading in worker pipe %d", pc_num, pc_num - 1);
        else {
            printf("Worker #%d: received command from client %d (%d) -- ind=%d\n", 
                    pc_num, command.pc_num, command.pid, ++command.ind);
            // send SIGURS1 to the client
            if (kill(command.pid, SIGUSR1) < 0)
                terminate(errno, "last worker sending SIGURS1 to %d", command.pid);
        }
    close(pipes[pc_num - 1][0]);
    exit(EXIT_SUCCESS);
}

void worker_task(int pc_num) {
    close(pipes[pc_num - 1][1]);
    close(pipes[pc_num][0]);
    int status;
    data command;
    while (status = read(pipes[pc_num - 1][0], &command, sizeof(data)) != 0)
        if (status < 0)
            terminate(errno, "worker %d reading in worker pipe %d", pc_num, pc_num - 1);
        else {
            printf("Worker #%d: received command from client %d (%d) -- ind=%d\n", 
                    pc_num, command.pc_num, command.pid, ++command.ind);
            if ((status = write(pipes[pc_num][1], &command, sizeof(data))) < 0)
                terminate(errno, "worker %d writing in worker pipe %d", pc_num, 0);
        }
    close(pipes[pc_num - 1][0]);
    close(pipes[pc_num][1]);
    exit(EXIT_SUCCESS);
}

// REFACTOR THAT
void worker(int pc_num) {
    // int status;
    if (pc_num == 0) {                  // first worker
        worker0_task(pc_num);
    } else if (pc_num == nbo - 1) {     // last worker
        worker_last_task(pc_num);
    } else {                            // workers strictly inside the chain
        worker_task(pc_num);
    }
}

void client(int pc_num) {
    pid_t cpid = getpid();
    close(pipe_cw[0]);
    data command = {.pid = cpid, .pc_num = pc_num, .ind = 0};
    printf("\tclient %d (%d): command sent, waiting...\n", pc_num, cpid);
    if (write(pipe_cw[1], &command, sizeof(data)) < 0) 
        terminate(errno, "client %d writing in pipe", pc_num);
    sigsuspend(&client_mask);     // wait for SIGUSR1
    // SIGURS1 received => command received
    printf("\tclient %d (%d): command received\n", pc_num, cpid);
    close(pipe_cw[1]);
    exit(EXIT_SUCCESS);
}

// what a client has to do when receiving SIGUSR1
void cmd_rcv_handler(int sig) {
    // just catch the signal
}


int main(int argc, char **argv) {
    struct sigaction cmd_rcv_act;
    cmd_rcv_act.sa_flags = 0;
    sigemptyset(&cmd_rcv_act.sa_mask);
    cmd_rcv_act.sa_handler = cmd_rcv_handler;

    // TODO: make sure SIGUSR1 is not blocked for clients

    if (sigaction(SIGUSR1, &cmd_rcv_act, NULL) < 0) 
        terminate(errno, "sigaction");

    if (argc != 3) {
        terminate(EXIT_FAILURE, "usage: %s <nb of workers> <nb of clients>", argv[0]);
    }
    
    nbo = atoi(argv[1]);
    nbc = atoi(argv[2]);

    // creating workers pipes
    for (int i = 0; i < nbo - 1; i++) {
        if (pipe(pipes[i]) < 0)
            terminate(errno, "creating worker pipe #%d", i);
    }
    // creating client -> worker 0 pipe
    if (pipe(pipe_cw) < 0)
        terminate(errno, "creating pipe client -> worker 0");

    for (int i = 0; i < nbo; i++) {
        switch (fork()) {
            case -1:
                terminate(errno, "fork worker #%d", i);
            case 0:
                close_wpipes(i);
                worker(i);
            default:;
        }
    }

    for (int i = 0; i < nbc; i++) {
        switch (fork()) {
            case -1:
                terminate(errno, "fork client #%d", i);
            case 0:
                close_cpipes();
                client(i);
            default:;
        }
    }
    close(pipe_cw[0]);
    close(pipe_cw[1]);
    for (int i = 0; i < nbo - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }


    // wait for all workers/clients to finish
    while (wait(NULL) != -1)
        ;

    puts("----<> end of program <>----");

    return EXIT_SUCCESS;
}
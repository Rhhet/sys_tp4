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


#define NBO_MAX 8
#define NBC_MAX 8

int nbo, nbc;
int pipes[NBO_MAX][2];  // pipes between workers

// sisgsuspend mask
sigset_t client_mask;

// client command request
typedef struct {
    pid_t pid;              // pid
    int pc_num;             // process number
    int ind;                // indicator
    int infos[NBO_MAX];     // information array (each cell should be modified by the corresponding worker)
} data;


void terminate(int errcode, const char *msg, ...) {
    va_list specifiers;
    va_start(specifiers, msg);
    vfprintf(stderr, msg, specifiers);
    fprintf(stderr, "\n%s", strerror(errno));
    va_end(specifiers);
    exit(errcode);
}

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
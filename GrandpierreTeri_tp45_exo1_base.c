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

#define MAX_PC_NB 5
#define MAX_MSG_LEN 100

typedef struct {
    int src_id;     // source pc identifier (father=-1)
    int cst;        // const
    int var;        // var to increment
    pid_t src_pid;  // source pc pid
} data;

// prototypes
static void terminate(const char *msg, ...);
static void child_routine(data *info, int pc_id, pid_t pid);
static void child0_routine(data *info, int pc_id, pid_t pid);
static bool rcv_info(data *info, int pc_id);
static bool mod_info(data *info, int pc_id, pid_t pid);
static bool print_info(data *info);
static bool snd_info(data *info, int pc_id);
static void close_pipes(int i);

// declare all pipes needed for the pcs
int pipes[MAX_PC_NB][2];
int pc_nb, loop_nb;

static void terminate(const char *msg, ...) {
    // TODO add vararg ... + modif exit arg (add param to terminate)
    fprintf(stderr, "error: %s\n", msg);
    exit(EXIT_FAILURE);
}

static void close_pipes(int pc_id) {
    // close pc_id proc's preceeding pipe (write)
    // following pipe (read)
    close(pipes[pc_id][1]);
    close(pipes[(pc_id + 1) % pc_nb][0]);
    // close all others pipes (read & write)
    for (int i = 0; i < pc_nb; i++) {
        if (i != pc_id)
            close(pipes[i][0]);
        if (i != (pc_id + 1) % pc_nb)
            close(pipes[i][1]);
    }
}

static void child_routine(data *info, int pc_id, pid_t pid) {
    while (rcv_info(info, pc_id)) {
        mod_info(info, pc_id, pid);  // modif info
        print_info(info);       // print info
        snd_info(info, pc_id);  // send info
    }
    exit(EXIT_SUCCESS);
}

static void child0_routine(data *info, int pc_id, pid_t pid) {
    for (int i = 0; i < loop_nb; i++) {
        rcv_info(info, pc_id);
        mod_info(info, pc_id, pid);  // modif info
        print_info(info);       // print info
        snd_info(info, pc_id);  // send info
    }
    exit(EXIT_SUCCESS);
}

// modif return of these fcts
static bool rcv_info(data *info, int pc_id) {
    int status;
    if ((status = read(pipes[pc_id][0], info, sizeof(data))) == -1)
        terminate("reading from pipe");
    return (status == sizeof(data)) ? true : false; 
}
// add pid to params
static bool mod_info(data *info, int pc_id, pid_t pid) {
    info->var++;
    info->src_pid = pid;
    info->src_id = pc_id;
    return true;
}

static bool print_info(data *info) {
    printf("proc #%d (pid=%d) -- var=%d\n", info->src_id, 
            info->src_pid, info->var);
    return true;
}

static bool snd_info(data *info, int pc_id) {
    if (write(pipes[(pc_id + 1) % pc_nb][1], info, sizeof(data)) == -1)
        terminate("writing in pipe");
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <nb of pcs> <nb of loops>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    pc_nb = atoi(argv[1]);
    loop_nb = atoi(argv[2]);
    pid_t pids[pc_nb];
    data info;

    // creating pipes
    for (int i = 0; i < pc_nb; i++) {
        if (pipe(pipes[i]) == -1) {
            terminate("creating pipe");
        }
    }    

    // initializing info
    info.cst = 999;     
    info.var = 0;       // increment var
    info.src_pid = getpid();    // source pc pid
    info.src_id = -1;    // source pc identifier

    puts("start working...");

    // main pc injects info in first pipe
    if (write(pipes[0][1], &info, sizeof(info)) == -1) {
        terminate("injecting msg into first pipe");
    }

    // creating pb_nb childs pcs
    for (int i = 0; i < pc_nb; i++) {
        switch (fork()) {
            case -1:
                terminate("creating child");
                break;
            case 0:     // child i does
                close_pipes(i);
                if (i)
                    child_routine(&info, i, getpid());
                else 
                    child0_routine(&info, i, getpid());
            default: ;
        }
    }

    for (int i = 0; i < pc_nb; i++) {
        close(pipes[i][0]);  // close pipes read
        close(pipes[i][1]);  // closes pipes write
    }

    while (wait(NULL) != -1)
        ;   // wait for all childs pc to terminate
    
    puts("end prgm");

    return EXIT_SUCCESS;
}
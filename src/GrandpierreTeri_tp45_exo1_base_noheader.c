/**
 * IMPORTANT: Un code beaucoup plus propre est disponible
 * sur github (https://github.com/Rhhet/sys_tp4). 
 * L'impossibilite de rentre plus de 3
 * fichiers empeche l'ecriture de header et d'autre fichiers
 * sources dans l'objcetif d'avoir un code bien mieux structure...
 * */


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

static void child_routine(data *info, int pc_id, pid_t ppid);
static void child0_routine(data *info, int pc_id, pid_t ppid);
static bool rcv_info(data *info, int pc_id);
static pid_t mod_info(data *info, int pc_id, pid_t pid);
static void print_info(data *info, pid_t pre_pid, pid_t ppid);
static void snd_info(data *info, int pc_id);
static void close_pipes(int i);

// declare all pipes needed for the pcs
int pipes[MAX_PC_NB][2];
int pc_nb, loop_nb;

static void terminate(int errcode, const char *msg, ...) {
    va_list specifiers;
    va_start(specifiers, msg);
    vfprintf(stderr, msg, specifiers);
    fprintf(stderr, "\n%s", strerror(errno));
    va_end(specifiers);
    exit(errcode);
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

static void child_routine(data *info, int pc_id, pid_t ppid) {
    pid_t pid = getpid();
    while (rcv_info(info, pc_id)) {
        pid_t pre_pid = mod_info(info, pc_id, pid);     // modif info
        print_info(info, pre_pid, ppid);                // print info
        snd_info(info, pc_id);                          // send info
    }
    printf("child #%d terminating\n", pc_id);
    close(pipes[pc_id][0]);
    close(pipes[(pc_id + 1) % pc_nb][1]);
    exit(EXIT_SUCCESS);
}

static void child0_routine(data *info, int pc_id, pid_t ppid) {
    pid_t pid = getpid();
    for (int i = 0; i < loop_nb; i++) {
        rcv_info(info, pc_id);
        pid_t pre_pid = mod_info(info, pc_id, pid);     // modif info
        print_info(info, pre_pid, ppid);                // print info
        snd_info(info, pc_id);                          // send info
    }
    printf("first child (#%d) terminating\n", 0);
    close(pipes[pc_id][0]);
    close(pipes[(pc_id + 1) % pc_nb][1]);
    exit(EXIT_SUCCESS);
}

// modif return of these fcts
static bool rcv_info(data *info, int pc_id) {
    int status;
    if ((status = read(pipes[pc_id][0], info, sizeof(data))) == -1)
        terminate(errno, "reading from pipe #%d", pc_id);
    return (status == sizeof(data)) ? true : false; 
}

static pid_t mod_info(data *info, int pc_id, pid_t pid) {
    pid_t pre_pid = info->src_pid;  // keep preceeding proc's pid
    info->var++;
    info->src_pid = pid;
    info->src_id = pc_id;
    return pre_pid;
}

static void print_info(data *info, pid_t pre_pid, pid_t ppid) {
    if (info->src_id == 0) {
        printf("proc #%d (pid=%d) -- var=%d received from proc #%d (pid=%d)\n", info->src_id, 
                info->src_pid, info->var, (pre_pid == ppid) ? -1: pc_nb - 1, 
                pre_pid);
    } else {
        printf("proc #%d (pid=%d) -- var=%d received from #%d (pid=%d)\n", info->src_id, 
                info->src_pid, info->var, (info->src_id - 1) % pc_nb, pre_pid);
    }
}

static void snd_info(data *info, int pc_id) {
    if (write(pipes[(pc_id + 1) % pc_nb][1], info, sizeof(data)) == -1)
        terminate(errno, "writing in pipe #%d", (pc_id + 1) % pc_nb);
}

int main(int argc, char **argv) {

    if (argc != 3) {
        terminate(EXIT_FAILURE, "usage: %s <nb of procs> <nb of loops>\n", argv[0]);
    }
    
    pc_nb = atoi(argv[1]);
    loop_nb = atoi(argv[2]);
    pid_t pids[pc_nb];
    data info;

    // creating pipes
    for (int i = 0; i < pc_nb; i++) {
        if (pipe(pipes[i]) == -1) {
            terminate(errno, "creating pipe #%d", i);
        }
    }    

    // initializing info
    pid_t ppid = getpid();

    info.cst = 999;     
    info.var = 0;       // increment var
    info.src_pid = ppid;    // source pc pid
    info.src_id = -1;    // source pc identifier

    printf("parent proc #%d (%d) starts... var=%d, const=%d\n", info.src_id, 
            info.src_pid, info.var, info.cst);

    // main pc injects info in first pipe
    if (write(pipes[0][1], &info, sizeof(info)) == -1) {
        terminate(errno, "injecting msg into first pipe");
    }

    // creating pb_nb childs pcs
    for (int i = 0; i < pc_nb; i++) {
        switch (fork()) {
            case -1:
                terminate(errno, "creating child #%d", i);
                break;
            case 0:     // child i does
                close_pipes(i);
                if (i)
                    child_routine(&info, i, ppid);
                else 
                    child0_routine(&info, i, ppid);
            default: ;
        }
    }

    for (int i = 1; i < pc_nb; i++) {
        close(pipes[i][0]);  // close pipes read
        close(pipes[i][1]);  // closes pipes write
    }
    close(pipes[0][1]);

    while (wait(NULL) != -1)
        ;   // wait for all childs pc to terminate

    if (read(pipes[0][0], &info, sizeof(data)) < 0)
        terminate(errno, "read in first pipe by parent proc");
    printf("parent proc get the info back from #%d: var=%d, const=%d\n", info.src_id, 
            info.var, info.cst);
    
    close(pipes[0][0]);  // let the last child write in the pipe (no SIGPIPE)
    puts("---<> end prgm <>---");

    return EXIT_SUCCESS;
}

/**
 * I make the parent proc close the pipe pipes[0][0] after the wait,
 * because the last child (#(pc_nb - 1)) has to write in this pipe 
 * while no one is reading form it (the child #0 closed it upon terminating)
 * only the parent proc can still read from it.
 * This caused the program to sometime terminate while the last child didn't
 * (this however is weird, the error EPIPE would have been raised
 * by the terminate() function and after numerous testings, i noticed it didn't,
 * the program simply terminate without letting the child pc_nb - 1 to write 
 * to stderr this error... ).
 * */
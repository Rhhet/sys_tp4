/**
 * IMPORTANT: Un code beaucoup plus propre est disponible
 * sur github (https://github.com/Rhhet/sys_tp4). 
 * L'impossibilite de rentre plus de 3
 * fichiers empeche l'ecriture de header et d'autre fichiers
 * sources dans l'objcetif d'avoir un code bien mieu structure...
 * */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include "proc_handling.h"

int pipes[MAX_PC_NB][2];
int pc_nb, loop_nb;

void terminate(int errcode, const char *msg, ...) {
    va_list specifiers;
    va_start(specifiers, msg);
    vfprintf(stderr, msg, specifiers);
    fprintf(stderr, "\n%s", strerror(errno));
    va_end(specifiers);
    exit(errcode);
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

    puts("start working...");

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

    for (int i = 0; i < pc_nb; i++) {
        close(pipes[i][0]);  // close pipes read
        close(pipes[i][1]);  // closes pipes write
    }

    while (wait(NULL) != -1)
        ;   // wait for all childs pc to terminate
    
    puts("end prgm");

    return EXIT_SUCCESS;
}
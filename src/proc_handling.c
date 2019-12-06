#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "proc_handling.h"

// declare all pipes needed for the pcs
extern int pipes[MAX_PC_NB][2];
extern int pc_nb, loop_nb;

extern void terminate(int errcode, const char *msg, ...);


void close_pipes(int pc_id) {
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

void child_routine(data *info, int pc_id, pid_t ppid) {
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

void child0_routine(data *info, int pc_id, pid_t ppid) {
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
bool rcv_info2(data *info, int pc_id) {
    int status;
    if ((status = read(pipes[pc_id][0], info, sizeof(data))) == -1)
        terminate(errno, "reading from pipe #%d", pc_id);
    return (status == sizeof(data)) ? true : false; 
}

bool rcv_info(data *info, int pc_id) {
    int status = read(pipes[pc_id][0], info, sizeof(data));
    int offset;
    switch (status) {
        case -1:
            terminate(errno, "reading from pipe #%d", pc_id);
        case sizeof(data):
            return true;
        case 0:
            return false;
        default:    // only a portion of the data has been read => continue to read
            offset = status;
            while (offset != sizeof(data)) {
                if ((status = read(pipes[pc_id][0], info + offset, sizeof(data) - offset)) == -1)
                    terminate(errno, "reading from pipe #%d", pc_id);
                offset += status;
            }
            return true;
    }
}

pid_t mod_info(data *info, int pc_id, pid_t pid) {
    pid_t pre_pid = info->src_pid;  // keep preceeding proc's pid
    info->var++;
    info->src_pid = pid;
    info->src_id = pc_id;
    return pre_pid;
}

void print_info(data *info, pid_t pre_pid, pid_t ppid) {
    if (info->src_id == 0) {
        printf("proc #%d (pid=%d) -- var=%d received from proc #%d (pid=%d)\n", info->src_id, 
                info->src_pid, info->var, (pre_pid == ppid) ? -1: pc_nb - 1, 
                pre_pid);
    } else {
        printf("proc #%d (pid=%d) -- var=%d received from #%d (pid=%d)\n", info->src_id, 
                info->src_pid, info->var, (info->src_id - 1) % pc_nb, pre_pid);
    }
}

void snd_info(data *info, int pc_id) {
    if (write(pipes[(pc_id + 1) % pc_nb][1], info, sizeof(data)) == -1)
        terminate(errno, "writing in pipe #%d", (pc_id + 1) % pc_nb);
}
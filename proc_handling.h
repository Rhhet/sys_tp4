/**
 * This header provides an interface to handle 
 * manipulation of the data structure for multiple
 * processes communicating via pipes
 */
#ifndef PROC_HANDLING_H
#define PROC_HANDLING_H

#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>

#define MAX_PC_NB 5

typedef struct {
    int src_id;     // source pc identifier (father=-1)
    int cst;        // const
    int var;        // var to increment
    pid_t src_pid;  // source pc pid
} data;

void child_routine(data *info, int pc_id, pid_t ppid);
void child0_routine(data *info, int pc_id, pid_t ppid);
bool rcv_info(data *info, int pc_id);
pid_t mod_info(data *info, int pc_id, pid_t pid);
void print_info(data *info, pid_t pre_pid, pid_t ppid);
void snd_info(data *info, int pc_id);
void close_pipes(int i);



#endif

#ifndef WORKERS_CLIENTS_H
#define WORKERS_CLIENTS_H

#include <unistd.h>

#define NBO_MAX 8
#define NBC_MAX 8

// client command request
typedef struct {
    pid_t pid;              // pid
    int pc_num;             // process number
    int ind;                // indicator
    int infos[NBO_MAX];     // information array (each cell should be modified by the corresponding worker)
} data;

void close_pipes(int pc_num);
void close_client_pipes();
void print_n_modify(int pc_num, data *command);
void worker(int pc_num);
void client(int pc_num);


#endif
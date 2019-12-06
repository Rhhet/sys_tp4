#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "print_msg.h"


int main(int argc, char **argv) {
    pid_t pid = getpid();

    while (1) {
        print_msg(pid);
    }
    
    return EXIT_SUCCESS;
}
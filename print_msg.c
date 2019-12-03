#include <stdio.h>
#include <unistd.h>


void print_msg(pid_t pid) {
	printf("Child proc (pid=%d): looping...\n", pid);
	sleep(1);
}

/**-------------------------------------------------------------------------
  TP4 - Squelette code exercice 2-V1
  Compilation : gcc tp45_exo2-v1_base.c afficher.o -o tp45_exo2-v1 -Wall
--------------------------------------------------------------------------**/

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "print_msg.h"

#define LOOP_NB 20

void terminate(int errcode, const char *msg, ...) {
    va_list specifiers;
    va_start(specifiers, msg);
    vfprintf(stderr, msg, specifiers);
    fprintf(stderr, "\n%s", strerror(errno));
    va_end(specifiers);
    exit(errcode);
}

/*------------------------------------------------------------------------
  SIGINT handling
  ------------------------------------------------------------------------*/
void sigint_handling(int sig) {
	printf(">> Ctrl-C/SIGINT received by %d\n", getpid());
}

/*------------------------------------------------------------------------
  Child process code
  ------------------------------------------------------------------------*/
void child_pc() {
	pid_t pid = getpid();
	/* while (1) {
		print_msg(pid);
	} */
	for (int i = 0; i < LOOP_NB; i++)
		print_msg(pid);
	exit(EXIT_SUCCESS);
}

/*------------------------------------------------------------------------*/
int main(int argc, char **argv) {
	pid_t cpid, ppid = getpid();
	struct sigaction sigint_act;

	// this flag ensures the wait primitive is restarted when
	// SIGINT is recevied by the parent proc
	sigint_act.sa_flags = SA_RESTART;
	sigint_act.sa_handler = sigint_handling;
	if (sigemptyset(&sigint_act.sa_mask) < 0)
		terminate(errno, "sigemptyset parent pc");
	
	if (sigaction(SIGINT, &sigint_act, NULL) < 0)
		terminate(errno, "sigaction parent pc");
	// parent proc is now SIGINT-protected (as is its child proc)

	// TODO make sure SIGINT is not blocked...

	printf("parent process (pid=%d): SIGINT-protected\n", ppid);
	printf("gpid=%d\n", getpgrp());
	switch (cpid = fork())	{
		case -1:
			terminate(errno, "fork");
		case 0:
			printf("gpid=%d\n", getpgrp());
			child_pc();
		/* default : break; */
	}

	printf("parent process (pid=%d): created child process (pid=%d)\n",
		   ppid, cpid);


	int status = wait(NULL);
	printf("wait status=%d\n", status);
	return EXIT_SUCCESS;
}

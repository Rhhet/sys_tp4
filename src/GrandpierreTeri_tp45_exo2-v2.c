/**-------------------------------------------------------------------------
  TP4 - Squelette code exercice 2-V2
  Compilation : gcc tp45_exo2-v2_base.c -o tp45_exo2-v2 -Wall
--------------------------------------------------------------------------**/

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <string.h>


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
void sigint_handler(int sig) {
	printf(">> Ctrl-C/SIGINT received by %d\n", getpid());
}

/*------------------------------------------------------------------------*/
int main(int argc, char **argv) {
	pid_t pid = getpid();
	struct sigaction sigint_act;

	sigint_act.sa_flags = 0;	
	sigint_act.sa_handler = sigint_handler;
	if (sigemptyset(&sigint_act.sa_mask) < 0)
		terminate(errno, "sigint sigemptyset");
	if (sigaction(SIGINT, &sigint_act, NULL) < 0)
	 	terminate(errno, "sigint sigaction");
	// main proc is know SIGINT-proctected

	// TODO unblock (if necesary) SIGINT

	printf("parent process (pid=%d): know SIGINT-protected\n", pid);
	sleep(3); 	// to have time to send SIGINT before parent proc executes loop

	printf("parent process (pid=%d): know executing <loop>\n", pid);

	execl("./exe/loop", "./exe/loop", NULL);
	
	puts("error returning form exec");
	return EXIT_FAILURE;
}

/**-------------------------------------------------------------------------
  TP5 - Squelette code exercice 3
  Compilation : gcc tp45_exo3_base.c -o tp45_exo3 -Wall
--------------------------------------------------------------------------**/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_LG 80

int msg_nb, period, cpt = 0;	// IMPORTANT: the number of messages is NOT used in the child routine
int pip[2];						// see line 43 for why i use msg_nb as a global variable

void terminate(int errcode, const char *msg, ...) {
    va_list specifiers;
    va_start(specifiers, msg);
    vfprintf(stderr, msg, specifiers);
    fprintf(stderr, "\n%s", strerror(errno));
    va_end(specifiers);
    exit(errcode);
}

/*-----------------------------
 * Envoyer un message a chaque fois que le delai est ecoule
 * ----------------------------*/
void snd_msg(int sigrcv) {
	/* Constituer le message */
	time_t t;
	char msg[MAX_LG];
	sprintf(msg, "%s%d %s %s", "message #", cpt++, "sent at", (time(&t), ctime(&t)));

	if (write(pip[1], msg, sizeof(msg)) < 0)
		terminate(errno, "write in pipe (iter=%d)", cpt - 1);

	if (cpt < msg_nb)	// this is an alternative to the sigprocmask(...) line 113
		alarm(period);	// we don't want the last SIGALRM to make the parent proc write
						// another and last message
}

/*-----------------------------*/
void chd_routine(void) {
	char msg[MAX_LG];
	int status;
	time_t t;
	while (1) {
		if ((status = read(pip[0], msg, sizeof(msg))) < 0)
			terminate(errno, "read in pipe");
		if (!status)
			break;
		printf("\tChild proc -- msg form parent proc: %s", msg);
	}
	printf("\tChild proc terminating at %s", (time(&t), ctime(&t)));
	close(pip[0]);
	exit(EXIT_SUCCESS);
}

/*-----------------------------*/
int main(int argc, char **argv) {
	struct sigaction alrm_act;
	alrm_act.sa_flags = 0;
	alrm_act.sa_handler = snd_msg;

	if (argc != 3) {
		terminate(EXIT_FAILURE, "Usage : %s <period (s)> <nb of msg>\n", argv[0]);
	}

	period = atoi(argv[1]);
	msg_nb = atoi(argv[2]);

	if (pipe(pip) < 0)
		terminate(errno, "pipe creation");

	switch (fork())	{
		case -1:
			terminate(errno, "fork");
		case 0:
			close(pip[1]);	// child dosen't write in the pipe
			chd_routine();
		default:
			break;
	}
	// parent proc only writes in pipe
	close(pip[0]);

	if (sigaction(SIGALRM, &alrm_act, NULL) < 0)
		terminate(errno, "sigaction");

	sigset_t alrm_msk, old_msk;
	if (sigemptyset(&alrm_msk) < 0)
		terminate(errno, "sigemptyset %s", "alarm mask");
	if (sigemptyset(&old_msk) < 0)
		terminate(errno, "sigemptyset %s", "old mask");
	if (sigaddset(&alrm_msk, SIGALRM) < 0)
		terminate(errno, "sigaddset");
	// unblock SIGALRM (if blocked)
	// keep the old mask to re-install it later
	if (sigprocmask(SIG_UNBLOCK, &alrm_msk, &old_msk) < 0)	
		terminate(errno, "sigprocmask unblock %d", SIGALRM);
	// SIGALRM not blocked from here				

	alarm(period);
	while (cpt < msg_nb) {
		;	// parent proc only does a loop here; could be some code (asynchronous treatment)
	}
	// sigprocmask(SIG_BLOCK, &alrm_msk, NULL);		!! cf line 44 !!
	close(pip[1]);
	if (sigprocmask(SIG_SETMASK, &old_msk, NULL) < 0)	// re-install old mask
		terminate(errno, "sigprocmask %s", "reinstall old mask");

	wait(NULL);		// maybe wait for the child to terminate
	puts("--<> end prgm <>--");

	return EXIT_SUCCESS;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <csse2310a3.h>
#include <unistd.h>

bool signalReceived = false; // flag to determine if a signal is received
FILE* output; // output stream of the program

/* signal_handler()
 * −−−−−−−−−−−−−−−
 * Handles the specified signal
 *
 * signo: signal number
 */
void signal_handler(int signo) {
    char* signal = strsignal(signo);
    fprintf(output, "sigcat received %s\n", signal);
    fflush(output);

    // Change output stream depending on received signal
    if (signo == SIGUSR1) {
	output = stdout;
    }
    if (signo == SIGUSR2) {
	output = stderr;
    }
}

int main(int argc, char** argv) {
    // Set up sigaction struct
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;

    // Set up sigaction to handle signals 1 to 31 except
    // SIGKILL and SIGSTOP
    for (int i = 1; i <= 31; i++) {
    	if (i != SIGKILL && i != SIGSTOP) {
    	    sigaction(i, &sa, 0);
    	}
    }
    output = stdout;
    // Main loop
    while (true) {
    	while (!signalReceived) {
	    char* line = read_line(stdin);
	    // Exit when EOF
	    if (line == NULL) {
    		return 0;
	    }
    	    fprintf(output, "%s\n", line);
	    fflush(output);
	    free(line);
	}
    	signalReceived = false;
    }
}

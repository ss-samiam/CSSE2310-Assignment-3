#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <csse2310a3.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MILLI_TO_SECONDS 1000000
#define BASE_10 10

/* A Job created in the program by spawn */
typedef struct Job {
    int jobId;
    pid_t processId;
    char* processName;
    int toChild;
    int fromChild;
    int exitStatus;
    int signalStatus;
    int exitCode;
    int eofStatus;
} Job;

/* add_job()
 * −−−−−−−−−−−−−−−
 * Adds a job with given elements to the job array.
 *
 * jobs: the array of jobs to add to
 * totalJob: current amount of jobs in the array
 * processId: pid of the job to add
 * name: process name of the job to add
 * toChild: file descriptor of pipe from parent (write) to child (read)
 * fromChild: file descriptor of pipe from child (write) to parent (read)
 */
void add_job(Job*** jobs, int* totalJobs, pid_t processId, char* name, 
	int toChild, int fromChild) {
    int id = *totalJobs;
    (*totalJobs)++;

    // Initialise and populate job elements
    Job* newJob = (Job*) malloc(sizeof(Job));
    newJob->jobId = id;
    newJob->processId = processId;
    newJob->processName = (char*) malloc((strlen(name) + 1) * sizeof(char));
    strcpy(newJob->processName, name);
    newJob->toChild = toChild;
    newJob->fromChild = fromChild;
    newJob->exitStatus = 0;
    newJob->signalStatus = 0;
    newJob->exitCode = 0;
    newJob->eofStatus = 0;
    
    // Add new job into the job array
    *jobs = realloc(*jobs, (*totalJobs) * sizeof(Job*));
    (*jobs)[id] = newJob;
}

/* close_old_pipes()
 * −−−−−−−−−−−−−−−
 * Closes duplicate pipes inherited from the parent.
 *
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs
 */
void close_old_pipes(Job*** jobs, int* totalJobs) {
    for (int i = 0; i < *totalJobs; i++) {
	Job* specifiedJob = (*jobs)[i];
	close(specifiedJob->toChild); 
	close(specifiedJob->fromChild); 
    }
}

/* command_spawn()
 * −−−−−−−−−−−−−−−
 * Handles the spawn command input by the user (runs a given program in a 
 * new process)
 *
 * arguments: commands entered by the user
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs 
 */
void command_spawn(char** arguments, Job*** jobs, int* totalJobs) {
    char* processName = arguments[0];
    if (processName == NULL) {
	printf("Error: Insufficient arguments\n");
	fflush(stdout);
	return;
    }
    int parentToChild[2], childToParent[2];
    pipe(parentToChild);
    pipe(childToParent);
    pid_t pid = fork();

    if (pid) {
	printf("New Job ID [%d] created\n", *totalJobs);
	fflush(stdout);

	add_job(jobs, totalJobs, pid, processName, 
		parentToChild[1], childToParent[0]);
	close(parentToChild[0]); // never read from to child
	close(childToParent[1]); // never write to from child
    } else {
	// Overwrite inherited signal handler back to default
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
    	sigaction(SIGINT, &sa, NULL);

	close_old_pipes(jobs, totalJobs); // close inherited duplicated pipes
	
	close(parentToChild[1]); // never write to from parent
	close(childToParent[0]); // never read from to parent

	dup2(parentToChild[0], STDIN_FILENO);
	close(parentToChild[0]);

	dup2(childToParent[1], STDOUT_FILENO);
	close(childToParent[1]);

	if (execvp(processName, arguments) == -1) {
	    exit(99);
	}
    }
}

/* digits_only()
 * −−−−−−−−−−−−−−−
 * Determines if a given string is a valid number
 * 
 * string: the string to check
 */
int digits_only(const char* string) {
    char* notDigits;
    strtol(string, &notDigits, BASE_10);
    
    if (string[0] == '\0' || *notDigits != '\0') {
	return 0;
    } else {
	return 1;
    }
}

/* decimals_only()
 * −−−−−−−−−−−−−−−
 * Determines if a given string is a valid decimal number
 * 
 * string: the string to check
 */
int decimals_only(const char* string) {
    char* notDigits;
    strtod(string, &notDigits);
    
    if (string[0] == '\0' || *notDigits != '\0') {
	return 0;
    } else {
	return 1;
    }
}

/* update_process_status()
 * −−−−−−−−−−−−−−−
 * Updates the struct with their exitStatus, signalStatus, and exitCode flags
 * 
 * job: the given job to update
 */
void update_process_status(Job* job) {
    int status;
    pid_t pid = job->processId;

    // Check if job is still running
    if (!job->exitStatus && !job->signalStatus) {

	if (waitpid(pid, &status, WNOHANG) != 0) {
	    if (WIFEXITED(status)) {
		job->exitStatus = 1;
		job->exitCode = WEXITSTATUS(status);
	    } else if (WIFSIGNALED(status)) {
		job->signalStatus = 1;
		job->exitCode = WTERMSIG(status);
	    }
	}
    }
}

/* print_process_status()
 * −−−−−−−−−−−−−−−
 * Prints the current status in the correct format
 *
 * job: the given job
 */
void print_process_status(Job* job) {
    update_process_status(job);

    if (job->exitStatus) { 
	printf("exited(%d)\n", job->exitCode);
    } else if (job->signalStatus) {
	printf("signalled(%d)\n", job->exitCode);
    } else {
	printf("running\n");
    }
}

/* command_report()
 * −−−−−−−−−−−−−−−
 * Handles the report command input by the user 
 * (report on the status of job/s)
 * 
 * arguments: commands entered by the user
 * numTokens: number of arguments
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs
 */
void command_report(char** arguments, int numTokens, 
	Job*** jobs, int* totalJobs) {
    Job* specifiedJob;
    // Check for enough arguments
    if (numTokens > 1) {
	// Check for valid Job ID
	if (digits_only(arguments[0]) && atoi(arguments[0]) < *totalJobs) {
	    specifiedJob = (*jobs)[atoi(arguments[0])];
	    printf("[Job] cmd:status\n");
	    printf("[%d] %s:", 
		    specifiedJob->jobId, specifiedJob->processName);
	    print_process_status(specifiedJob);
	} else {
	    printf("Error: Invalid job\n");
	    return;
	}
    // Report on all jobs when no Job is specified
    } else {
	printf("[Job] cmd:status\n");
	for (int i = 0; i < *totalJobs; i++) {
	    specifiedJob = (*jobs)[i];
	    printf("[%d] %s:", 
		    specifiedJob->jobId, specifiedJob->processName);
	    print_process_status(specifiedJob);
	}
    }
    fflush(stdout);
    return;
}

/* command_signal()
 * −−−−−−−−−−−−−−−
 * Handles the signal command entered by the user 
 * (sends a given signal to the given job)
 *
 * arguments: commands entered by the user
 * numTokens: number of arguments
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs
 */
void command_signal(char** arguments, int numTokens, 
	Job*** jobs, int* totalJobs) {
    // Check for enough arguments
    if (numTokens >= 3) {
	// Check for valid Job ID
	if (digits_only(arguments[0]) && atoi(arguments[0]) < *totalJobs) {
	    Job* specifiedJob = (*jobs)[atoi(arguments[0])];
	    // Check for valid signal number
	    if (digits_only(arguments[1]) && atoi(arguments[1]) >= 
		    SIGHUP && atoi(arguments[1]) <= SIGSYS) {
		int specifiedSignal = atoi(arguments[1]);
		// Send the signal to the specified job
		if (!specifiedJob->exitStatus && 
			!specifiedJob->signalStatus) {
		    pid_t pid = specifiedJob->processId; 
		    kill(pid, specifiedSignal);
		}
	    } else {
		printf("Error: Invalid signal\n");
	    }

	} else {
	    printf("Error: Invalid job\n");
	}    
    } else {
	printf("Error: Insufficient arguments\n");
    }
    fflush(stdout);
    return;
}

/* command_sleep()
 * −−−−−−−−−−−−−−−
 * Handles the sleep command entered by the user 
 * (causes hq to sleep for the number of seconds specified)
 *
 * arguments: commands entered by the user
 * numTokens: number of arguments
 */
void command_sleep(char** arguments, int numTokens) {
    // Check for enough arguments
    if (numTokens > 1) {
	// Check if time given is valid (i.e. a decimal number)
	if (decimals_only(arguments[0])) {
	    char* endTime;
	    double sleepTime = strtod(arguments[0], &endTime);
	    // Converts milliseconds to seconds
	    usleep(sleepTime * MILLI_TO_SECONDS);
	} else {
	    printf("Error: Invalid sleep time\n");
	}
    } else {
	printf("Error: Insufficient arguments\n");
    }
    fflush(stdout);
    return;
}

/* command_send()
 * −−−−−−−−−−−−−−−
 * Handles the send command entered by the user 
 * (sends a given text to the given job)
 *
 * arguments: commands entered by the user
 * numTokens: number of arguments
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs
 */
void command_send(char** arguments, int numTokens, 
	Job*** jobs, int* totalJobs) {
    // Check for enough arguments
    if (numTokens >= 3) {
	// Check for valid Job ID
	if (digits_only(arguments[0]) && atoi(arguments[0]) < *totalJobs) {
	    Job* specifiedJob = (*jobs)[atoi(arguments[0])];
	    update_process_status(specifiedJob);

	    // Check if Job is still running, 
	    // if so, its file descriptor is valid to write to
	    if (!specifiedJob->exitStatus && !specifiedJob->signalStatus) {

		// Make string to send new-line terminated so 
		// read_line() doesn't hang
		int writeToChild = specifiedJob->toChild;
		char* stringToSend 
			= malloc(strlen(arguments[1] + 1) * sizeof(char));
		strcpy(stringToSend, arguments[1]);
		strcat(stringToSend, "\n");
		// Write to child
		write(writeToChild, stringToSend, strlen(stringToSend));
		free(stringToSend);
	    }
	} else {
	    printf("Error: Invalid job\n");
	}
    } else {
	printf("Error: Insufficient arguments\n");
    }
    return;
}

/* command_rcv()
 * −−−−−−−−−−−−−−−
 * Handles the rcv command entered by the user 
 * (Attempt to read one line of text from the job specified 
 * and display it to hq’s stdout)
 *
 * arguments: commands entered by the user
 * numTokens: number of arguments
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs
 */
void command_rcv(char** arguments, int numTokens, 
	Job*** jobs, int* totalJobs) {
    // Check for enough arguments
    if (numTokens > 1) {
	// Check for valid Job ID
	if (digits_only(arguments[0]) && atoi(arguments[0]) < *totalJobs) {
	    Job* specifiedJob = (*jobs)[atoi(arguments[0])];
	    int readFromChild = specifiedJob->fromChild;

	    // Check if specified job has eof'd before
	    if (specifiedJob->eofStatus == 0) { 

		// Print output from child when there is data to be read
	    	if (is_ready(readFromChild)) {
	    	    FILE* fromChildStream = fdopen(readFromChild, "r");
	    	    char* line = read_line(fromChildStream);
		    
		    // Change eofStatus to true when EOF is read
	    	    if (line != NULL) {
	    		printf("%s\n", line);
	    	    } else {
	    		specifiedJob->eofStatus = 1;
	    	    }
		} else {
		    printf("<no input>\n");
		}
	    // Prints <EOF> if job has eof'd before
	    } 
	    if (specifiedJob->eofStatus) {
		printf("<EOF>\n");
	    }
	} else {
	    printf("Error: Invalid job\n");
	}
    } else {
	printf("Error: Insufficient arguments\n");
    }
    return;
}

/* command_eof()
 * −−−−−−−−−−−−−−−
 * Handles the eof command entered by the user 
 * (Close the pipe connected to the specified job’s stdin)
 *
 * arguments: commands entered by the user
 * numTokens: number of arguments
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs
 */
void command_eof(char** arguments, int numTokens, 
	Job*** jobs, int* totalJobs) {
    // Check for enough arguments
    if (numTokens > 1) {
	// Check for valid Job ID
	if (digits_only(arguments[0]) && atoi(arguments[0]) < *totalJobs) {
	    
	    // Close communication between parent to child therefore eof
	    Job* specifiedJob = (*jobs)[atoi(arguments[0])];
	    close(specifiedJob->toChild); 
	} else {
	    printf("Error: Invalid job\n");
	}
    } else {
	printf("Error: Insufficient arguments\n");
    }
    return;
}

/* command_cleanup()
 * −−−−−−−−−−−−−−−
 * Handles the cleanup command entered by the user 
 * (Terminate and reap all child processes spawned by hq)
 *
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs
 */
void command_cleanup(Job*** jobs, int* totalJobs) {
    // Sends SIGKILL to every job
    for (int i = 0; i < *totalJobs; i++) {	
	Job* specifiedJob = (*jobs)[i];
	
	// Check if job hasn't exited yet
	if (!specifiedJob->exitStatus && !specifiedJob->signalStatus) {
	    int status;
	    pid_t specifiedPid = specifiedJob->processId;
	    kill(specifiedPid, SIGKILL);
	    waitpid(specifiedPid, &status, 0);
	    specifiedJob->signalStatus = 1;
	    specifiedJob->exitCode = WTERMSIG(status);
	}
    }
    return;
}
	
/* process_input()
 * −−−−−−−−−−−−−−−
 * Determine which command is entered by the user and call 
 * the appropriate function to handle
 * 
 * input: command inputs by the user
 * numTokens: number of arguments
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs
 */
void process_input(char** input, int numTokens, Job*** jobs, int* totalJobs) {
    char* command = input[0];
    char** arguments = input + 1; // Removes user command from parsed inputs
    
    if (strcmp(command, "spawn") == 0) {
	command_spawn(arguments, jobs, totalJobs);
    } else if (strcmp(command, "report") == 0) {
    	command_report(arguments, numTokens, jobs, totalJobs);
    } else if (strcmp(command, "signal") == 0) {
	command_signal(arguments, numTokens, jobs, totalJobs);
    } else if (strcmp(command, "sleep") == 0) {
    	command_sleep(arguments, numTokens);
    } else if (strcmp(command, "send") == 0) {
	command_send(arguments, numTokens, jobs, totalJobs);
    } else if (strcmp(command, "rcv") == 0) {
	command_rcv(arguments, numTokens, jobs, totalJobs);
    } else if (strcmp(command, "eof") == 0) {
	command_eof(arguments, numTokens, jobs, totalJobs);
    } else if (strcmp(command, "cleanup") == 0) {
	command_cleanup(jobs, totalJobs);
    } else {
	printf("Error: Invalid command\n");
    }
}

/* free_jobs()
 * −−−−−−−−−−−−−−−
 * Frees every pointer assigned to the job struct array
 * 
 * jobs: the array of all jobs
 * totalJobs: total amount of jobs
 */
void free_jobs(Job** jobs, int* totalJobs) {
    for (int i = 0; i < *totalJobs; i++) {
	free(jobs[i]->processName);
	free(jobs[i]);
    }
    free(jobs);
}

int main(int argc, char** argv) {
    // Setup sigaction to ignore SIGINT
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa, NULL);

    Job** jobs = NULL;
    int totalJobs = 0;
    int numTokens;

    // Main loop of program
    while (true) {
    	printf("> ");   
	fflush(stdout);
	char* line = read_line(stdin);

	// Exit on EOF, cleanup all jobs and free memory
	if (line == NULL) {
	    command_cleanup(&jobs, &totalJobs);
	    free_jobs(jobs, &totalJobs);
	    return 0;
	}
	// Pass and process command inputs
	char** tokens = split_space_not_quote(line, &numTokens);
	if (tokens != NULL) {
	    process_input(tokens, numTokens, &jobs, &totalJobs);
	}
	free(line);
    	free(tokens);
    }
}

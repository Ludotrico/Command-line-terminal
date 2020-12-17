#include "icssh.h"
#include "linkedList.h"
#include <readline/readline.h>
#include "helpers.h"

static int sigChildFlag = 0;

void sigChildHandler(int sig) {
	sigChildFlag = 1;
	//printf("changed sig child flag\n");
}

void sigUserHandler(int sig) {
	printf("Hi User! I am process %d\n", getpid());
}

int main(int argc, char* argv[]) {
	int exec_result=0;
	int exit_status=0;
	pid_t pid=0;
	pid_t wait_result=0;
	char* line=NULL;

	List_t* bgList = malloc(sizeof(List_t));
    bgList->comparator = NULL;
    bgList->length = 0;
    bgList->head = NULL;

	signal(SIGCHLD, sigChildHandler);
	signal(SIGUSR2, sigUserHandler);

#ifdef GS
    rl_outstream = fopen("/dev/null", "w");
#endif

	// Setup segmentation fault handler
	if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {	
		perror("Failed to set signal handler");
		exit(EXIT_FAILURE);
	}

    // print the prompt & wait for the user to enter commands string
	while ((line = readline(SHELL_PROMPT)) != NULL) {
        // MAGIC HAPPENS! Command string is parsed into a job struct
        // Will print out error message if command string is invalid
		job_info* job = validate_input(line);
        if (job == NULL) { // Command was empty string or invalid
			free(line);
			continue;
		}

        //Prints out the job linked list struture for debugging
        #ifdef DEBUG   // If DEBUG flag removed in makefile, this will not longer print
            debug_print_job(job);
        #endif

		// example built-in: exit
		if (strcmp(job->procs->cmd, "exit") == 0) {
			// Terminating the shell

			node_t * head = bgList->head;
			while (head != NULL) {
				bgentry_t* bg = (bgentry_t*) head->value;
				if(bg) {
					printf(BG_TERM, bg->pid, bg->job->line);
					kill(bg->pid, SIGKILL);
				}
				head = head->next;
			}

			free(line);
			free_job(job);
            validate_input(NULL);
			cleanList(bgList);
            return 0;
		}

		if (sigChildFlag) {
			reapProcesses(bgList, &sigChildFlag);
		}

		if(handleBGPipes(job,bgList) || handleBG(job, bgList) || handleBGLIST(job, bgList)) {
			debugPrint("handledBG\n");
		} else {
			// example of good error handling!
			if ((pid = fork()) < 0) {
				perror("fork error");
				exit(EXIT_FAILURE);
			}
			if (pid == 0) {  //If zero, then it's the child process
				//get the first command in the job list

				if (handleCD(job) || handleESTATUS(job, WEXITSTATUS(exit_status)) || handlePipes(job) || handleArt(job)) 
					return 0;


				if (!handleRedirection(job->procs)) 
					return 0;

				debugPrint("Child foreground\n");
				proc_info* proc = job->procs;
				exec_result = execvp(proc->cmd, proc->argv);

				if (exec_result < 0) {  //Error checking
					printf(EXEC_ERR, proc->cmd);
					
					// Cleaning up to make Valgrind happy 
					// (not necessary because child will exit. Resources will be reaped by parent or init)
					free_job(job);  
					free(line);
					validate_input(NULL);
				
					exit(EXIT_FAILURE);
				}
			} else {
				// As the parent, wait for the foreground job to finis
				wait_result = waitpid(pid, &exit_status, 0);

				if (wait_result < 0) {
					printf(WAIT_ERR);
					exit(EXIT_FAILURE);
				}
			}
	}
		//free_job(job);  // if a foreground job, we no longer need the data
		debugPrint("before free\n");
		free(line);
	}

    // calling validate_input with NULL will free the memory it has allocated
    validate_input(NULL);
	cleanList(bgList);

#ifndef GS
	fclose(rl_outstream);
#endif
	return 0;
}

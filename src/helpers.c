// Your helper functions need to be here.


#include "helpers.h"
#include <unistd.h> 
#include <time.h>
#include "linkedList.h"
#include <errno.h>


int handleCD(job_info* job) {
    if (strcmp(job->procs->cmd, "cd") == 0) {
        char s[200];
        if (job->procs->argc > 1) {
            if (chdir(job->procs->argv[1]) != 0) {
                fprintf(stderr, "%s", DIR_ERR);
                return 1;
            }
        } else {
            if (chdir(getenv("HOME")) != 0) {
                fprintf(stderr, "%s", DIR_ERR);
                return 1;
            }
        }
        printf("%s\n", getcwd(s, 200));
        return 1;

    }
    return 0;
    
}

int handleESTATUS(job_info* job, int exitStatus) {
    if (strcmp(job->procs->cmd, "estatus") == 0) {
        printf("%d\n", exitStatus);
        return 1;
    }
    return 0;
}

int handleBG(job_info* job, List_t * bgList) {
    if (!job->bg)
      return 0;

    debugPrint("Handle BG\n");

    int pid=0;
    int exec_result=0;

    if ((pid = fork()) < 0) {
        perror("fork error");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {  //If zero, then it's the child process
        debugPrint("inside BG child\n");
        proc_info* proc = job->procs;
        debugPrint("%s\n",proc->cmd);

        handleRedirection(proc);
        exec_result = execvp(proc->cmd, proc->argv);
        debugPrint("execvp failed!\n");

        if (exec_result < 0) {  //Error checking
            printf(EXEC_ERR, proc->cmd);
            
            // Cleaning up to make Valgrind happy 
            // (not necessary because child will exit. Resources will be reaped by parent or init)
            free_job(job);  
            validate_input(NULL);
        
            exit(EXIT_FAILURE);
        }
    }

    // pid is the child process id, but here we are running the parent code
    bgentry_t* newBG =  malloc(sizeof(bgentry_t));
    newBG->pid = pid;
    newBG->job = job;   
    newBG->seconds = time(NULL);

    debugPrint("about to insert rear!\n");
    printList(bgList, 'c');
    insertRear(bgList, newBG);

    debugPrint("about to print list\n");
    printList(bgList, 'c');

    return 1;
}

int handleBGLIST(job_info* job, List_t* bgList) {
    if (strcmp(job->procs->cmd, "bglist") == 0) {
        debugPrint("bglist length: %d\n", bgList->length);
        node_t* head = bgList->head;
        while (head != NULL) {
            debugPrint("One entry %p\n",(void*)head);

            print_bgentry((bgentry_t*) head->value);
            head = head->next;
        }
        return 1;
    }
    return 0;
}

void reapProcesses(List_t* bgList, int * flag) {
    if(bgList==NULL)
        return;

    debugPrint("Entering Reap\n");

    node_t * head = bgList->head;
    int index = 0;

    while (head != NULL) {
        bgentry_t * bg = (bgentry_t*)(head->value);
        
        if(bg!=NULL) {
            int status=0;
            int val = waitpid(bg->pid  , &status, WNOHANG);
            if ( val != 0  ) {
                printf(BG_TERM, bg->pid, bg->job->line);
                removeByIndex(bgList,  index); 
            }
        }
        
        head = head->next;
        index++;
    }

    
    if (flag && bgList->length == 0) {
        *flag = 0;
    }
    

}

int StrCmp(char*s1,char*s2)
{
    if(!s1)
        return 0;
    if(!s2)
        return 0;
    return !strcmp(s1,s2);
}


int handleRedirection(proc_info * proc) {
    int err=StrCmp(proc->in_file, proc->out_file);
    err+=StrCmp(proc->in_file, proc->err_file);
    err+=StrCmp(proc->out_file, proc->err_file);
    err+=StrCmp(proc->in_file, proc->outerr_file);
    err+=StrCmp(proc->outerr_file, proc->out_file);
    err+=StrCmp(proc->outerr_file, proc->err_file);

    debugPrint("passed sanity %d\n", err);
    if(err) {
        fprintf(stderr, RD_ERR);
        return 0;
    }

    if (proc->in_file != NULL) {
        int newIn = open(proc->in_file, O_RDONLY);
        if(newIn!=-1)
           dup2(newIn, STDIN_FILENO);
        else {
            fprintf(stderr, RD_ERR);
            return 0;
        }
        
    }

    if (proc->outerr_file != NULL) {
        int newOE = open(proc->outerr_file, O_WRONLY| O_CREAT | O_TRUNC);
        if(newOE!=-1) {
            dup2(newOE, STDERR_FILENO);
            dup2(newOE, STDOUT_FILENO);
        }
    } else { 
        if (proc->out_file != NULL) {
            debugPrint("before newO: %s\n",proc->out_file );
            int newO = creat(proc->out_file, 0644);
            debugPrint("after newO %d %s\n", newO, strerror(errno));
            if(newO!=-1) {
                int err=dup2(newO, STDOUT_FILENO);
                debugPrint("after dup2 %d %s\n", err, strerror(errno));
                if(err!=-1)
                    close(newO);
            }
        } 
        if (proc->err_file != NULL) {
            int newE = open(proc->err_file, O_WRONLY| O_CREAT | O_TRUNC);
            if(newE>0)
                dup2(newE, STDERR_FILENO);
        }
    }
    return 1;


    /*	char *in_file;                // name of file that stdin redirects from
	char *out_file;               // name of file that stdout redirects to
	char *err_file;               // name of file that stderr redirects to
	char *outerr_file; */
 }


int handleBGPipes(job_info * job, List_t* bgList) {
    if ((job->nproc > 1) && job->bg) {
        int pid=0;
        if (  (pid = fork()) == 0 ) {
            //Child
            int i = 1;
            int exec_result;
            struct proc_info * head = job->procs;

            while ( i < job->nproc)
            {
                int pd[2];
                
                pipe(pd);

                if (!fork()) {
                    dup2(pd[1], 1); // remap output back to parent

                    handleRedirection(head);
                    exec_result = execvp(head->cmd, head->argv);

                    if (exec_result < 0) {  //Error checking
                        printf(EXEC_ERR, head->cmd);
                        
                        // Cleaning up to make Valgrind happy 
                        // (not necessary because child will exit. Resources will be reaped by parent or init)
                        free_job(job);  
                        validate_input(NULL);

                    
                        exit(EXIT_FAILURE);
                    }
                }

                // remap output from previous child to input
                dup2(pd[0], 0);
                close(pd[1]);

                i++;
                head = head->next_proc;
            }

            handleRedirection(head);
            exec_result = execvp(head->cmd, head->argv);

            if (exec_result < 0) {  //Error checking
                printf(EXEC_ERR, head->cmd);
                
                // Cleaning up to make Valgrind happy 
                // (not necessary because child will exit. Resources will be reaped by parent or init)
                free_job(job);  
                validate_input(NULL);

            
                exit(EXIT_FAILURE);
            }
            return 1;
        }
        //parent
        bgentry_t* newBG =  malloc(sizeof(bgentry_t));
        newBG->pid = pid;
        newBG->job = job;   
        newBG->seconds = time(NULL);

        debugPrint("about to insert rear!\n");
        printList(bgList, 'c');
        insertRear(bgList, newBG);

        return 1;
    }
    return 0;
}

 int handlePipes(job_info * job) {
    if (job->nproc > 1) {
        int i = 1;
        int exec_result;
        struct proc_info * head = job->procs;

        while ( i < job->nproc)
        {
            int pd[2];
            
            pipe(pd);

            if (!fork()) {
                dup2(pd[1], 1); // remap output back to parent
                handleRedirection(head);
                exec_result = execvp(head->cmd, head->argv);

                if (exec_result < 0) {  //Error checking
                    printf(EXEC_ERR, head->cmd);
                    
                    // Cleaning up to make Valgrind happy 
                    // (not necessary because child will exit. Resources will be reaped by parent or init)
                    free_job(job);  
                    validate_input(NULL);

                
                    exit(EXIT_FAILURE);
                }
            }

            // remap output from previous child to input
            dup2(pd[0], 0);
            close(pd[1]);

            i++;
            head = head->next_proc;
        }
        
        handleRedirection(head);
        exec_result = execvp(head->cmd, head->argv);

        if (exec_result < 0) {  //Error checking
            printf(EXEC_ERR, head->cmd);
            
            // Cleaning up to make Valgrind happy 
            // (not necessary because child will exit. Resources will be reaped by parent or init)
            free_job(job);  
            validate_input(NULL);

        
            exit(EXIT_FAILURE);
        }
        return 1;
    }
    return 0;
 }

 int handleArt(job_info * job) {
     if (strcmp(job->procs->cmd, "ascii53") == 0) {
         printf("%s\n", "░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░\n░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░\n░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░\n░░░░░░░░░░░░░░░░░░░░░░░░░░░████░░░░░░░░░\n░░░░░░░███████░░░░░░░░░░████░░░██░░░░░░░\n░░░░████░░░░░███░░░░░░░██░░░░░░░██░░░░░░\n░░░██░░░░░░░░░░██░░░░░██░░░░░░░░░█░░░░░░\n░░░█░░░░░░░░░░░░██░░░██░░░░░░░░░░██░░░░░\n░░░█░░░░░░░░░░░░░████░░░░░░░░░░░░░█░░░░░\n░░░█░░░░░█░░░░█░░░██░░░░░░░░░░░░░░█░░░░░\n░░░█░░░░░█░░░░█░░░░░░░░░░░░░░░░░░░█░░░░░\n░░░█░░░░░█░░░░█░░░████░░░░██░░░░░██░░░░░\n░░░█░░░░░█░░░░█░░██░░░░░░░██░░░░██░░░░░░\n░░░██░░░░█░░░░█░░█░░░░░░░░██░░░░█░░░░░░░\n░░░░██░░░██████░░█░░░░░░░░██░░░██░░░░░░░\n░░░░░██░░██████░░██████░░░██░░██░░░░░░░░\n░░░░░░██░░░░░░░░░░░░░░░░░░░░░██░░░░░░░░░\n░░░░░░░███░░░░░░░░░░░░░░░░░░░█░░░░░░░░░░\n░░░░░░░░░██░░░░░░░░░░░░░░░░░█░░░░░░░░░░░\n░░░░░░░░░░██░░░░░░░░░░░░░░░██░░░░░░░░░░░\n░░░░░░░░░░░██░░░░░░░░░░░░░██░░░░░░░░░░░░\n░░░░░░░░░░░░░██░░░░░░░░░███░░░░░░░░░░░░░\n░░░░░░░░░░░░░░███░░░░░███░░░░░░░░░░░░░░░\n░░░░░░░░░░░░░░░░██░░░░█░░░░░░░░░░░░░░░░░\n░░░░░░░░░░░░░░░░░██████░░░░░░░░░░░░░░░░░\n░░░░░░░░░░░░░░░░░░░██░░░░░░░░░░░░░░░░░░░\n░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░");
         return 1;
     }
     return 0;
 }

 void cleanList(List_t* bgList) {
    node_t * head = bgList->head;

    while (head != NULL) {
        node_t * tmp = head->next;
        free(head->value);
        free(head);
        head = tmp;
    }
    free(bgList);
 }
// A header file for helpers.c
// Declare any additional functions in this file

#include "icssh.h"
#include "linkedList.h"
#include <stdarg.h>

#define debugPrint(fmt, ...) \
            do { if(0) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

int handleCD(job_info* job);
int handleESTATUS(job_info* job, int exitStatus);
int handleBG(job_info* job, List_t* bgList);
int handleBGLIST(job_info* job, List_t* bgList);
void reapProcesses(List_t* bgList, int * flag);
int handleRedirection(proc_info*);
int handlePipes(job_info * job);
int handleBGPipes(job_info*, List_t*);
int handleArt(job_info *job);
void cleanList(List_t* bgList);
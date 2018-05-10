#ifndef FIRST_MAIN_H
#define FIRST_MAIN_H

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <malloc.h>

#include "common.h"
#include "pa2345.h"
#include "ipc.h"
#include "banking.h"
#include "functions.h"

#define OUT 1
#define IN 0
#define MODE "w"

typedef struct {
	int **procFds;
	int started;
	int done;
	local_id id;
	int balance;
} ProcessInfo;

extern int numberOfProcesses;
extern FILE *eventFileLogFd, *pipeFileLogFd;
local_id realIdOfProcess(local_id selfId, local_id procId);
local_id programIdOfProcess(local_id selfId, local_id procId);
int processRequest(ProcessInfo *info, Message *msg);

enum {
	READ_ERROR_FMT,
	WRITE_ERROR_FMT
};

#endif //FIRST_MAIN_H

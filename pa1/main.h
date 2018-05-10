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

#include "common.h"
#include "pa1.h"
#include "ipc.h"

#define OUT 1
#define IN 0
#define MODE "w"

extern int numberOfProcesses;
extern FILE *eventFileLogFd, *pipeFileLogFd;
local_id realIdOfProcess(local_id selfId, local_id procId);

enum {
	READ_ERROR_FMT,
	WRITE_ERROR_FMT
};

#endif //FIRST_MAIN_H

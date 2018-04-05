#include "main.h"

const char *key = "-p";
const char *usageHelp = "Usage:\n\t$(command) -p $(number of processes)\n";
const char *keyArgHelp = "number of processes should be positive number between 1 and %d\n";
const char *parentChildPipesOpened = "Parent <- Child №%d pipes opened.\tWrite: %d\tRead: %d\n";
const char *childPipesOpened = "Child №%d -> Child №%d pipes opened.\tWrite: %d\tRead: %d\n";

FILE *eventFileLogFd, *pipeFileLogFd;
char outputBuffer[4096];

int numberOfProcesses;

void doSomethingUseful() {
	//not this time
	return;
}

void parseInputKey(int argc, char **argv){
	int number;

	if(argc < 3){
		fprintf(stderr, usageHelp);
		exit(-1);
	}
	if(strcmp(key, argv[1]) != 0) {
		fprintf(stderr, usageHelp);
		exit(-2);
	}
	number = atoi(argv[2]);
	if(number <= 0 || number > MAX_PROCESS_ID) {
		fprintf(stderr, keyArgHelp, MAX_PROCESS_ID);
		exit(-3);
	}
	numberOfProcesses = number;
}

void duplicateOutputToTerminal(FILE *filePointer, char *outputString, int error){
	fprintf(filePointer, outputString);
	if (error == 0) {
		fprintf(stdout, outputString);
	} else {
		fprintf(stderr, outputString);
	}
	fflush(filePointer);
}

void printIOError(local_id id, local_id procId, int operationCode) {
	switch(operationCode) {
		case READ_ERROR_FMT:
			sprintf(outputBuffer, "ERROR: read error in %d from %d with code: %d, msg: %s\n", id, procId, errno, strerror(errno));
			duplicateOutputToTerminal(eventFileLogFd, outputBuffer, 1);
			break;
		case WRITE_ERROR_FMT:
			sprintf(outputBuffer, "ERROR: write error in %d to %d with code: %d, msg: %s\n", id, procId, errno, strerror(errno));
			duplicateOutputToTerminal(eventFileLogFd, outputBuffer, 1);
			break;
	}
	fflush(eventFileLogFd);
	exit(-1);
}

void processRequest(local_id id, int *startedCount, int *doneCount, Message *msg, void *self) {
	int resCode = receive_any(self, msg);
	if (resCode != 0) {
		printIOError(id, realIdOfProcess(id, -resCode), READ_ERROR_FMT);
	} else {
		switch (msg->s_header.s_type) {
			case STARTED:
				(*startedCount)--;
				break;
			case DONE:
				(*doneCount)--;
				break;
			default:
				break;
		}
	}
}

void childProcess(int (*procFds)[2], pid_t parent, pid_t self, local_id id) {

	int startedMessages = numberOfProcesses - 1, doneMessages = numberOfProcesses - 1;

	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
	message.s_header.s_type = STARTED;
	sprintf (message.s_payload, log_started_fmt, id, self, parent);
	message.s_header.s_payload_len = strlen(message.s_payload);

	duplicateOutputToTerminal(eventFileLogFd, message.s_payload, 0);

	int resCode = send_multicast(procFds, &message);

	if (resCode != 0) {
		printIOError(id, realIdOfProcess(id, resCode), WRITE_ERROR_FMT);
	}

	while (startedMessages > 0) {
		processRequest(id, &startedMessages, &doneMessages, &message, procFds);
	}

	sprintf(outputBuffer, log_received_all_started_fmt, id);
	duplicateOutputToTerminal(eventFileLogFd, outputBuffer, 0);

	doSomethingUseful();

	message.s_header.s_magic = MESSAGE_MAGIC;
	message.s_header.s_type = DONE;
	sprintf (message.s_payload, log_done_fmt, id);
	message.s_header.s_payload_len = strlen(message.s_payload);

	duplicateOutputToTerminal(eventFileLogFd, message.s_payload, 0);

	resCode = send_multicast(procFds, &message);

	if (resCode != 0) {
		printIOError(id, realIdOfProcess(id, resCode), READ_ERROR_FMT);
	}

	while (doneMessages > 0) {
		processRequest(id, &startedMessages, &doneMessages, &message, procFds);
	}

	sprintf(outputBuffer, log_received_all_done_fmt, id);
	duplicateOutputToTerminal(eventFileLogFd, outputBuffer, 0);
	exit(0);
}

local_id realIdOfProcess(local_id selfId, local_id procId) {
	if (selfId > procId) {
		return procId;
	} else {
		return procId + 1;
	}
}

int programIdOfProcess(local_id selfId, local_id procId) {
	return selfId >= procId ? selfId -1 : selfId;
}


void prepareLogFiles() {
	eventFileLogFd = fopen(events_log, MODE);
	pipeFileLogFd = fopen(pipes_log, MODE);

	if (eventFileLogFd == NULL || pipeFileLogFd == NULL) {
		fprintf(stderr, "One of log files cannot be opened\n");
		exit(-4);
	}
}

void closePipe(int id, pid_t processId, int pipeFd) {
	close(pipeFd);
	fprintf(pipeFileLogFd, "Process with id: %d\tPid: %d\tclosed fd %d\n", id, processId, pipeFd);
}

int main(int argc, char* argv[]) {
	prepareLogFiles();
	parseInputKey(argc, argv);

	int pipesMatrix[numberOfProcesses + 1][numberOfProcesses][2];

	for (local_id i = 1; i <= numberOfProcesses; ++i) {
		for (local_id j = 0; j < numberOfProcesses; ++j) {
			if (pipe2(pipesMatrix[i][j], O_NONBLOCK) == -1) {
				perror("pipe");
				return errno;
			}
			fcntl(pipesMatrix[i][j][IN], F_SETPIPE_SZ, 1048576);
			fcntl(pipesMatrix[i][j][OUT], F_SETPIPE_SZ, 1048576);
			if (j == PARENT_ID){
				fprintf(pipeFileLogFd, parentChildPipesOpened, i, pipesMatrix[i][PARENT_ID][OUT], pipesMatrix[i][PARENT_ID][IN]);
			} else {
				fprintf(pipeFileLogFd, childPipesOpened, i, realIdOfProcess(i, j), pipesMatrix[i][j][OUT], pipesMatrix[i][j][IN]);
			}
		}
	}
	pid_t cpid, selfPid = getpid();

	//Flush all data before spawn child processes
	fflush(pipeFileLogFd);

	for (local_id i = 1; i <= numberOfProcesses; ++i) {
		dup2((long)eventFileLogFd, (long)stdout);
		cpid = fork();
		if (cpid == -1) {
			perror("fork");
			exit(-5);
		}
		if (cpid == 0){
			cpid = getpid();
			//Pass only used FDs to function
			int procFds[numberOfProcesses][2];
			for (int j = 1; j <= numberOfProcesses; ++j) {
				for (int k = 0; k < numberOfProcesses; ++k) {
					if(j == i) {
						procFds[k][OUT] = pipesMatrix[j][k][OUT];
					} else {
						closePipe(i, cpid, pipesMatrix[j][k][OUT]);
						if (programIdOfProcess(i, j) != k) {
							//closePipe(i, cpid, pipesMatrix[j][k][IN]);
						} else {
							procFds[programIdOfProcess(j, i)][IN] = pipesMatrix[j][k][IN];
						}
					}
				}
			}
			fflush(pipeFileLogFd);
			childProcess(procFds, selfPid, cpid, i);
			break;
		}
	}
	int procFds[numberOfProcesses + 1][2];
	procFds[0][OUT] = -1;
	for (local_id j = 1; j <= numberOfProcesses; ++j) {
		for (local_id k = 0; k < numberOfProcesses; ++k) {
			if (k != PARENT_ID) {
				closePipe(PARENT_ID, selfPid, pipesMatrix[j][k][IN]);
			} else {
				procFds[j][IN] = pipesMatrix[j][k][IN];
			}
			closePipe(PARENT_ID, selfPid, pipesMatrix[j][k][OUT]);
		}
	}
	fflush(pipeFileLogFd);

	Message message;
	int startedMessages = numberOfProcesses, doneMessages = numberOfProcesses;

	while (startedMessages > 0) {
		processRequest(PARENT_ID, &startedMessages, &doneMessages, &message, procFds);
	}
	fprintf(eventFileLogFd, log_received_all_started_fmt, PARENT_ID);
	fflush(eventFileLogFd);

	while (doneMessages > 0) {
		processRequest(PARENT_ID, &startedMessages, &doneMessages, &message, procFds);
	}
	fprintf(eventFileLogFd, log_received_all_done_fmt, PARENT_ID);
	fflush(eventFileLogFd);

	for (int i = 0; i < numberOfProcesses; ++i) {
		wait(NULL);
	}
	return 0;
}


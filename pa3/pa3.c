#include "main.h"

const char *key = "-p";
const char *usageHelp = "Usage:\n\t$(command) -p $(number of processes)\n";
const char *keyArgHelp = "number of processes should be positive number between 1 and %d\n";
const char *parentChildPipesOpened = "Parent <- Child №%d pipes opened.\tWrite: %d\tRead: %d\n";
const char *childPipesOpened = "Child №%d -> Child №%d pipes opened.\tWrite: %d\tRead: %d\n";

FILE *eventFileLogFd, *pipeFileLogFd;
char outputBuffer[4096];

int numberOfProcesses;

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount) {
    TransferOrder order = {src, dst, amount};
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = TRANSFER;
    memcpy(msg.s_payload, &order, sizeof(order));
    msg.s_header.s_payload_len = sizeof(order);
    increaseLamportTime();
    send(((ProcessInfo*)parent_data)->procFds, src - 1, &msg);
    while(processRequest((ProcessInfo*)parent_data, &msg) != ACK);
}

int *parseInputKey(int argc, char **argv){
    int number;

    if(argc < 5){
        fprintf(stderr, "%s", usageHelp);
        exit(-1);
    }
    if(strcmp(key, argv[1]) != 0) {
        fprintf(stderr, "%s", usageHelp);
        exit(-2);
    }
    number = atoi(argv[2]);
    if(number <= 0 || number > MAX_PROCESS_ID) {
        fprintf(stderr, keyArgHelp, MAX_PROCESS_ID);
        exit(-3);
    }
    numberOfProcesses = number;
    int *balance = calloc(number, sizeof(int));
    for (int i = 0; i < number; i++) {
        balance[i] = atoi(argv[3 + i]);
    }
    return balance;
}

void duplicateOutputToTerminal(FILE *filePointer, char *outputString, int error){
    fprintf(filePointer, "%s", outputString);
    if (error == 0) {
        fprintf(stdout, "%s", outputString);
    } else {
        fprintf(stderr, "%s", outputString);
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

void updateHistory(int balance, BalanceHistory *history, int change) {
    timestamp_t time = get_lamport_time();
    for (int i = history->s_history_len; i < time; i++) {
        history->s_history[i].s_balance = balance;
        history->s_history[i].s_time = i;
    }
    balance += change;
    history->s_history[time].s_balance = balance;
    history->s_history[time].s_time = time;
    history->s_history_len = time + 1;
}

int processRequest(ProcessInfo *info, Message *msg) {
    int resCode = receive_any(info->procFds, msg);
    if (resCode != 0) {
        printIOError(info->id, programIdOfProcess(info->id, -resCode), READ_ERROR_FMT);
    } else {
        TransferOrder order = {0,0,0};
        switch (msg->s_header.s_type) {
            case STARTED:
                info->started--;
                break;
            case DONE:
                info->done--;
                break;
            case STOP:
                info->stop = 1;
                break;
            case TRANSFER: {
                memcpy(&order, msg->s_payload, sizeof(order));
                BalanceHistory *history = (BalanceHistory*)info->structure;
                if (order.s_src == info->id) {
                    updateHistory(info->balance, history, -order.s_amount);
                    timestamp_t time = get_lamport_time();
                    history->s_history[time].s_balance_pending_in += order.s_amount;
                    history->s_history[time + 1].s_balance_pending_in += order.s_amount;

                    info->balance -= order.s_amount;

                    increaseLamportTime();
                    send(info->procFds, programIdOfProcess(order.s_dst, info->id), msg);

                    sprintf(outputBuffer, log_transfer_out_fmt, get_lamport_time(), info->id, order.s_amount, order.s_dst);
                    duplicateOutputToTerminal(eventFileLogFd, outputBuffer, 0);
                } else if (order.s_dst == info->id) {
                    updateHistory(info->balance, history, order.s_amount);
                    info->balance += order.s_amount;

                    msg->s_header.s_type = ACK;
                    msg->s_header.s_payload_len = 0;

                    increaseLamportTime();
                    send(info->procFds, 0, msg);

                    sprintf(outputBuffer, log_transfer_in_fmt, get_lamport_time(), info->id, order.s_amount, order.s_src);
                    duplicateOutputToTerminal(eventFileLogFd, outputBuffer, 0);
                }
                break;
            }
            case BALANCE_HISTORY: {
                AllHistory *all = (AllHistory*) info->structure;
                BalanceHistory *history = &(all->s_history[all->s_history_len]);

                size_t size = 0;
                memcpy(&history->s_id, msg->s_payload, sizeof(history->s_id));
                size += sizeof(history->s_id);
                memcpy(&history->s_history_len, msg->s_payload + size, sizeof(history->s_history_len));
                size += sizeof(history->s_history_len);

                memcpy(history->s_history, msg->s_payload + size, msg->s_header.s_payload_len - size);
                all->s_history_len++;
                break;
            }
            default:
                break;
        }
    }
    return msg->s_header.s_type;
}

void childProcess(pid_t parent, pid_t self, ProcessInfo *info) {

    Message message;
    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_type = STARTED;
    sprintf (message.s_payload, log_started_fmt, get_lamport_time(), info->id, self, parent, info->balance);
    message.s_header.s_payload_len = strlen(message.s_payload);

    duplicateOutputToTerminal(eventFileLogFd, message.s_payload, 0);

    int resCode = send_multicast(info->procFds, &message);

    if (resCode != 0) {
        printIOError(info->id, realIdOfProcess(info->id, resCode), WRITE_ERROR_FMT);
    }

    while (info->started > 0) {
        processRequest(info, &message);
    }

    sprintf(outputBuffer, log_received_all_started_fmt, get_lamport_time(), info->id);
    duplicateOutputToTerminal(eventFileLogFd, outputBuffer, 0);

    while (!info->stop) {
        processRequest(info, &message);
    }

    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_type = DONE;
    sprintf (message.s_payload, log_done_fmt, get_lamport_time(), info->id, info->balance);
    message.s_header.s_payload_len = strlen(message.s_payload);

    duplicateOutputToTerminal(eventFileLogFd, message.s_payload, 0);

    resCode = send_multicast(info->procFds, &message);

    if (resCode != 0) {
        printIOError(info->id, realIdOfProcess(info->id, resCode), READ_ERROR_FMT);
    }

    while (info->done > 0) {
        processRequest(info, &message);
    }

    sprintf(outputBuffer, log_received_all_done_fmt, get_lamport_time(), info->id);
    duplicateOutputToTerminal(eventFileLogFd, outputBuffer, 0);

    BalanceHistory *history = (BalanceHistory *)info->structure;
    updateHistory(info->balance, history, 0);

    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_type = BALANCE_HISTORY;
    size_t size = 0;
    memcpy(message.s_payload, &history->s_id, sizeof(history->s_id));
    size += sizeof(history->s_id);
    memcpy(message.s_payload + size, &history->s_history_len, sizeof(history->s_history_len));
    size += sizeof(history->s_history_len);
    memcpy(message.s_payload + size, history->s_history, sizeof(BalanceState) * history->s_history_len);
    size += sizeof(BalanceState) * history->s_history_len;

    message.s_header.s_payload_len = size;
    increaseLamportTime();
    send(info->procFds, 0, &message);

    exit(0);
}

local_id realIdOfProcess(local_id selfId, local_id procId) {
    if (selfId > procId) {
        return procId;
    } else {
        return procId + 1;
    }
}

local_id programIdOfProcess(local_id selfId, local_id procId) {
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
    int *balance = parseInputKey(argc, argv);

    int pipesMatrix[numberOfProcesses + 1][numberOfProcesses][2];

    for (local_id i = 0; i <= numberOfProcesses; ++i) {
        for (local_id j = 0; j < numberOfProcesses; ++j) {
            if (pipe2(pipesMatrix[i][j], O_NONBLOCK) == -1) {
                perror("pipe");
                return errno;
            }
            if(i != 0){
                if (j == PARENT_ID){
                    fprintf(pipeFileLogFd, parentChildPipesOpened, i, pipesMatrix[i][PARENT_ID][OUT], pipesMatrix[i][PARENT_ID][IN]);
                } else {
                    fprintf(pipeFileLogFd, childPipesOpened, i, realIdOfProcess(i, j), pipesMatrix[i][j][OUT], pipesMatrix[i][j][IN]);
                }
            }
        }
    }
    pid_t cpid, selfPid = getpid();

    //Flush all data before spawn child processes
    fflush(pipeFileLogFd);

    for (local_id i = 1; i <= numberOfProcesses; ++i) {
        cpid = fork();
        if (cpid == -1) {
            perror("fork");
            exit(-5);
        }
        if (cpid == 0){
            cpid = getpid();
            //Pass only used FDs to function
            int procFds[numberOfProcesses][2];
            for (int j = 0; j <= numberOfProcesses; ++j) {
                for (int k = 0; k < numberOfProcesses; ++k) {
                    if (j == i) {
                        procFds[k][OUT] = pipesMatrix[j][k][OUT];
                        closePipe(i, cpid, pipesMatrix[j][k][IN]);
                    } else {
                        closePipe(i, cpid, pipesMatrix[j][k][OUT]);
                        if (programIdOfProcess(i, j) != k) {
                            closePipe(i, cpid, pipesMatrix[j][k][IN]);
                        } else {
                            procFds[programIdOfProcess(j, i)][IN] = pipesMatrix[j][k][IN];
                        }
                    }
                }
            }
            fflush(pipeFileLogFd);
            BalanceHistory history;
            history.s_id = i;
            history.s_history_len = 0;
            for (int i = 0; i <= MAX_T; i++) {
                history.s_history[i].s_balance_pending_in = 0;
            }

            ProcessInfo info = {(int**)procFds, numberOfProcesses - 1, numberOfProcesses - 1, 0, i, balance[i - 1], &history};
            childProcess(selfPid, cpid, &info);
            break;
        }
    }

    int procFds[numberOfProcesses][2];

    for (local_id j = 0; j <= numberOfProcesses; ++j) {
        for (local_id k = 0; k < numberOfProcesses; ++k) {
            if (j == PARENT_ID) {
                procFds[k][OUT] = pipesMatrix[j][k][OUT];
                closePipe(PARENT_ID, cpid, pipesMatrix[j][k][IN]);
            } else {
                closePipe(PARENT_ID, cpid, pipesMatrix[j][k][OUT]);
                if (PARENT_ID != k) {
                    closePipe(PARENT_ID, cpid, pipesMatrix[j][k][IN]);
                } else {
                    procFds[j - 1][IN] = pipesMatrix[j][k][IN];
                }
            }
        }
    }

    AllHistory all;

    ProcessInfo info = {(int**)procFds, numberOfProcesses, numberOfProcesses, 0, PARENT_ID, 0, &all};

    fflush(pipeFileLogFd);

    Message message;

    while (info.started > 0) {
        processRequest(&info, &message);
    }

    sprintf(outputBuffer, log_received_all_started_fmt, get_lamport_time(), info.id);
    duplicateOutputToTerminal(eventFileLogFd, outputBuffer, 0);
    fflush(eventFileLogFd);

    bank_robbery(&info, numberOfProcesses);

    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_type = STOP;
    sprintf (message.s_payload, "");
    message.s_header.s_payload_len = 0;

    int resCode = send_multicast(info.procFds, &message);

    if (resCode != 0) {
        printIOError(PARENT_ID, realIdOfProcess(PARENT_ID, resCode), READ_ERROR_FMT);
    }

    while (info.done > 0) {
        processRequest(&info, &message);
    }
    fprintf(eventFileLogFd, log_received_all_done_fmt, get_lamport_time(), info.id);
    fflush(eventFileLogFd);

    while (all.s_history_len < numberOfProcesses) {
        processRequest(&info, &message);
    }
    print_history(&all);

    for (int i = 0; i < numberOfProcesses; ++i) {
        wait(NULL);
    }
    return 0;
}

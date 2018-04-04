#include "functions.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int numberOfProcesses;
FILE *eventFileLogFd, *pipeFileLogFd;

int send(void * self, local_id dst, const Message * msg) {
	int fd = ((int(*)[2])self)[dst][OUT];

	if (msg->s_header.s_magic != MESSAGE_MAGIC) {
		return -1;
	}

	if(write(fd, msg, MAX_MESSAGE_LEN) == -1){
		return -1;
	}

	return 0;
}

int send_multicast(void * self, const Message * msg) {
	for (local_id i = 0; i < numberOfProcesses; i++) {
		int resultCode = send(self, i, msg);
		if (resultCode != 0) {
			return -i;
		}
	}
	return 0;
}

int receive(void * self, local_id from, Message * msg){
	int fd = ((int (*)[2])self)[from][IN];

	msg->s_header.s_magic = MESSAGE_MAGIC + 1;

	int readAmount;

	readAmount = read(fd, msg, MAX_MESSAGE_LEN);

	if (readAmount > 0){
		int nbytes;
		ioctl(fd, FIONREAD, &nbytes);
		fprintf(eventFileLogFd, "Read amount: %d\n", readAmount);
		fprintf(eventFileLogFd, "IOCTL: %d\n", nbytes);
		fflush(eventFileLogFd);
	}

	if(readAmount == -1) {
		return -1;
	}

	if (msg->s_header.s_magic != MESSAGE_MAGIC) {
		return -2;
	}

	return 0;
}

int receive_any(void * self, Message * msg) {
	int resultCode = -1;
	int length = ((int (*)[2])self)[0][OUT] == -1 ? numberOfProcesses + 1 : numberOfProcesses;
	while(resultCode != 0) {
		for (local_id i = 1; i < length; ++i) {
			resultCode = receive(self, i, msg);
			if(resultCode == 0) break;
			if(resultCode == -1 && errno != EAGAIN) {
				return -i;
			}
		}
	}
	return resultCode;
}


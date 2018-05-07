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

	if(write(fd, msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) == -1){
		return -1;
	}

	return 0;
}

int send_multicast(void * self, const Message * msg) {
	local_id id = 0;
	while (id < numberOfProcesses) {
		int resultCode = send(self, (int)id, msg);
		if (resultCode == 0) {
			++id;
		} else if (errno != EAGAIN) {
				return -id;
		}
	}
	return 0;
}

int receive(void * self, local_id from, Message * msg){
	int fd = ((int (*)[2])self)[from][IN];
	int readAmount;

	readAmount = read(fd, msg, sizeof(MessageHeader));

	if(readAmount == -1) {
		return -1;
	}

	if(readAmount > 0) {
		readAmount = read(fd, msg->s_payload, msg->s_header.s_payload_len);
		msg->s_payload[msg->s_header.s_payload_len] = 0;
	} else {
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
        sleep(1);
	}

	return resultCode;
}


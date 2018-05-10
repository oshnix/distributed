#include "functions.h"

timestamp_t lamportTime = 0;

int numberOfProcesses;
FILE *eventFileLogFd, *pipeFileLogFd;

int send(void * self, local_id dst, const Message * msg) {
	int fd = ((int(*)[2])self)[dst][OUT];

	Message *message = (Message*) msg;
	message->s_header.s_local_time = get_lamport_time();

	if (msg->s_header.s_magic != MESSAGE_MAGIC) {
		return -1;
	}

	if(write(fd, msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) == -1){
		return -1;
	}

	return 0;
}

int send_multicast(void * self, const Message * msg) {
	increaseLamportTime();
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
	} else if (readAmount == 0) {
		return -2;
	}

	if(readAmount > 0 && msg->s_header.s_payload_len > 0) {
		readAmount = read(fd, msg->s_payload, msg->s_header.s_payload_len);
		if (msg->s_header.s_type == STARTED || msg->s_header.s_type == DONE)
			msg->s_payload[msg->s_header.s_payload_len] = 0;
	}
	if (msg->s_header.s_local_time > get_lamport_time()) {
		setLamportTime(msg->s_header.s_local_time);
	}
	increaseLamportTime();
	return 0;
}

int receive_any(void * self, Message * msg) {
	int resultCode = -1;

	while(resultCode != 0) {
		for (local_id i = 0; i < numberOfProcesses; ++i) {
			resultCode = receive(self, i, msg);
			if(resultCode == 0) break;
			if(resultCode == -1 && errno != EAGAIN) {
				return -i;
			}
		}
		usleep(1);
        //usleep(100);
	}

	return resultCode;
}

timestamp_t get_lamport_time() {
	return lamportTime;
}

void increaseLamportTime() {
	lamportTime += 1;
}

void setLamportTime(timestamp_t time) {
	lamportTime = time;
}


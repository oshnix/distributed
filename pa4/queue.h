#ifndef PA4_QUEUE
#define PA4_QUEUE

#include <stdio.h>

#include "ipc.h"

typedef struct {
	local_id id;
	timestamp_t time;
} requestInfo;

typedef struct {
	requestInfo array[MAX_PROCESS_ID];
	int start;
	int length;
} queueStruct;

int findPositionAndAddElement(queueStruct *queue, requestInfo *info);
void removeFirstElement(queueStruct *queue);

#endif //PA4_QUEUE

#include "queue.h"

int getArrayIndex(int index) {
	return index % MAX_PROCESS_ID;
}

void addElementToPosition(queueStruct *queue, requestInfo *info, int index) {
	for (int i = queue->start + queue->length; i > index ; i--) {
		queue->array[getArrayIndex(i)] = queue->array[getArrayIndex(i - 1)];
	}
	requestInfo *arr_info = &(queue->array[getArrayIndex(index)]);
	arr_info->id = info->id;
	arr_info->time = info->time;
	queue->length += 1;
}

int findPositionAndAddElement(queueStruct *queue, requestInfo *info) {
	int i;
	for (i = queue->start; i < queue->start + queue->length; i++) {
		requestInfo arr_info = queue->array[getArrayIndex(i)];

		if (arr_info.time > info->time || (arr_info.time == info->time && arr_info.id > info->id)) {
			break;
		}
	}
	addElementToPosition(queue, info, i);
	return i;
}

void removeFirstElement(queueStruct *queue) {
	queue->start = getArrayIndex(queue->start + 1);
	queue->length -= 1;
}





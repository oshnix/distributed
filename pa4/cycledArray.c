#include "cycledArray.h"

int getArrayIndex(int index) {
	return index % MAX_PROCESS_ID;
}

void addElementToPosition(cycledArray *array, local_id id, int index) {
	for (int i = array->start + array->length; i > index ; i++) {
		array->array[getArrayIndex(i)] = array->array[getArrayIndex(i - 1)];
	}
	array->array[getArrayIndex(index)] = id;
	array->length += 1;
}





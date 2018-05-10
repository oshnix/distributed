#ifndef PA4_CYCLEDARRAY_H
#define PA4_CYCLEDARRAY_H

#include "ipc.h"

typedef struct {
	local_id array[MAX_PROCESS_ID];
	int start;
	int length;
} cycledArray;




#endif //PA4_CYCLEDARRAY_H

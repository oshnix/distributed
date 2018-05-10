#ifndef FIRST_FUNCTIONS_H
#define FIRST_FUNCTIONS_H


#include "main.h"

#include <inttypes.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "common.h"
#include "pa2345.h"
#include "ipc.h"
#include "banking.h"

void increaseLamportTime();
void setLamportTime(timestamp_t time);

#endif //FIRST_FUNCTIONS_H

#ifndef PROCESS_COMM_H
#define PROCESS_COMM_H
#include <stdio.h>
#include "queue.h"

int process_command(char *command,int newsock, queue *task_queue,FILE *log_file);

#endif
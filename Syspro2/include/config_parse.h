#ifndef CONFIG_PARSE_H
#define CONFIG_PARSE_H
#include <stdio.h>
#include <time.h>
#include <queue.h>

int parse_config(FILE *config_file,FILE *log_file,QueuedTask *tasks,int *task_count);

#endif
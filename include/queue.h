#ifndef QUEUE_H
#define QUEUE_H
#include <pthread.h>
#include "defines.h"

typedef struct{
    char source_host[MAX_PATH];
    int source_port;
    char source_dir[MAX_PATH];
    char source_file[MAX_PATH]; //store each file in the struct to associate it with each source_dir.
    char target_host[MAX_PATH];
    int target_port;
    char target_dir[MAX_PATH];
    char target_file[MAX_PATH];
    int active;
}QueuedTask;


typedef struct queue{
    QueuedTask *data;//data for queue
    int start;
    int end;
    int count;
    int max_count; //buffersize
    pthread_mutex_t lockqueue; //safe thread queue
    pthread_cond_t not_full;
    pthread_cond_t not_empty; //condition var queue
}queue;

extern queue task_queue;


void init_queue(queue *q,int max_count);
int push(queue *q,QueuedTask task);
QueuedTask pop(queue *q);

#endif
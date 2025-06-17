#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "../include/queue.h"
#include "defines.h"
extern volatile sig_atomic_t shutdown_flag;

int push(queue *q, QueuedTask task){
    pthread_mutex_lock(&q->lockqueue);
    while(q->count >= q->max_count && !shutdown_flag){
        printf(">>Found Buffer Full\n");
        pthread_cond_wait(&q->not_full,&q->lockqueue);
    }
    if(shutdown_flag){
        pthread_mutex_unlock(&q->lockqueue);
        return -1;
    }
    q->end = (q->end + 1) % q->max_count;
    q->data[q->end] = task;
    q->count++;
    pthread_cond_signal(&q->not_empty); //wakeup a worker
    pthread_mutex_unlock(&q->lockqueue);
    return 0;
}
void init_queue(queue *q, int max_count){
    q->start = 0;
    q->end = -1;
    q->count = 0;
    q->data = malloc(max_count * sizeof(QueuedTask));
    q->max_count = max_count; //save buffersize
    pthread_mutex_init(&q->lockqueue,NULL);
    pthread_cond_init(&q->not_empty,NULL);
    pthread_cond_init(&q->not_full,NULL);
}

QueuedTask pop(queue *q){
    QueuedTask task;
    pthread_mutex_lock(&q->lockqueue);
    while(q->count <= 0 && !shutdown_flag){
        printf(">> Found buffer empty\n");
        pthread_cond_wait(&q->not_empty,&q->lockqueue);
    }
    if(shutdown_flag && q->count <= 0){
        pthread_mutex_unlock(&q->lockqueue);
        memset(&task,0,sizeof(task));//empty task
        return task;
    }
    task = q->data[q->start];
    memset(&q->data[q->start], 0, sizeof(QueuedTask));
    q->start = (q->start + 1) % q->max_count;
    q->count--;
    pthread_cond_signal(&q->not_full);//tell to manager that we have space in the queue
    pthread_mutex_unlock(&q->lockqueue);
    return task;
}
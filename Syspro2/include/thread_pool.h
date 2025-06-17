#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

typedef struct{
    pthread_t *threads;//pinakas me thread ids
    int num_threads;//how many threads exei to pool
}thread_pool_t;

#endif
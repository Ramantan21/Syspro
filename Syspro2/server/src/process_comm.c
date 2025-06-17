#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> 
#include <pthread.h>
#include "../include/defines.h"
#include "../include/timer.h"
#include "../include/queue.h"
extern volatile sig_atomic_t shutdown_flag;
extern queue task_queue;
extern void send_list(queue *q,QueuedTask *tasks,int task_count);
extern void print_queued_task(const QueuedTask *task);

static void handle_add_command(char *src,char *trg,int newsock,queue *task_queue, FILE *log_file){
    char *src_dir = strtok(src,"@");
    char *source_hostport = strtok(NULL,"@");
    if(!src_dir || !source_hostport){
        fprintf(stderr,"Invalid src dir or host..\n");
        return;
    }
    if(src_dir[0] == '/') src_dir++;
    char *src_host = strtok(source_hostport,":");
    char *src_port_str = strtok(NULL,":");
    
    if(!src_host || !src_port_str){
        fprintf(stderr,"Invalid source host or port..\n");
        return;
    }

    int src_port = atoi(src_port_str);
    char *trg_dir = strtok(trg,"@");

    if(trg_dir[0] == '/') trg_dir++;
    char *trg_hostport = strtok(NULL,"@");
    if(!trg_dir || !trg_hostport){
        fprintf(stderr,"Invalid target dir or hostport..\n");
        return;
    }

    char *trg_host = strtok(trg_hostport,":");
    char *trg_port_str = strtok(NULL,":");

    if(!trg_host || !trg_port_str){
        fprintf(stderr,"Invalid trg_host or port..\n");
        return;
    }
    int already_active = 0;
    int trg_port = atoi(trg_port_str);
    for(int i = task_queue->start; i != task_queue->end; i = (i+1) % task_queue->max_count){
        QueuedTask *task = &task_queue->data[i];
        if(strcmp(task->source_dir,src_dir) == 0 && strcmp(task->source_host,src_host) == 0 &&
                task->source_port == src_port && task->active){
                already_active = 1;
                char buffer[1024];
                snprintf(buffer,sizeof(buffer),"[%s] Already in queue: /%s/%s@%s:%d\n",
                    timestamp(),task->source_dir,task->source_file,task->source_host,task->source_port);
                write(newsock,buffer,strlen(buffer));
        }
    }
    if(already_active) return;
    QueuedTask task = {0};
    strncpy(task.source_dir,src_dir,MAX_PATH - 1);
    task.source_dir[MAX_PATH - 1] = '\0';
    strncpy(task.source_host,src_host,MAX_PATH - 1);
    task.source_host[MAX_PATH - 1] = '\0';
    task.source_port = src_port;

    strncpy(task.target_dir, trg_dir,MAX_PATH - 1);
    task.target_dir[MAX_PATH - 1] = '\0';
    strncpy(task.target_host,trg_host,MAX_PATH - 1);
    task.target_host[MAX_PATH - 1] = '\0';
    task.target_port = trg_port;
    task.active = 1;
    send_list(task_queue, &task, 1);

    char buffer[1024];

    snprintf(buffer,sizeof(buffer),"[%s] Added to queue: /%s@%s:%d  /%s@%s:%d\n",
    timestamp(),src_dir,src_host,src_port,trg_dir,trg_host,trg_port);

    write(newsock,buffer,strlen(buffer));

    fprintf(log_file,"[%s] Added to queue: /%s@%s:%d  /%s@%s:%d\n",
    timestamp(),src_dir,src_host,src_port,trg_dir,trg_host,trg_port);
    fflush(log_file);

}

static void handle_cancel_command(char *src,int newsock,queue *task_queue,FILE *log_file){
    int found = 0;
    char buffer[1024];
    if(src[0] == '/') src++;
    pthread_mutex_lock(&task_queue->lockqueue);
    for(int i = task_queue->start; i != task_queue->end; i = (i+1) % task_queue->max_count){
        QueuedTask *task = &task_queue->data[i];
        if(strcmp(task->source_dir,src) == 0 && task->active){
            found = 1;
            task->active = 0;
            snprintf(buffer,sizeof(buffer),"[%s] Synchronization stopped for /%s@%s:%d\n",timestamp(),task->source_dir,task->source_host,task->source_port);
            write(newsock,buffer,strlen(buffer));
            break;
        }
    }
    pthread_mutex_unlock(&task_queue->lockqueue);
    if(!found){
        char buffer[1024];
        snprintf(buffer,sizeof(buffer),"[%s] Directory not being synchronized: %s\n",timestamp(),src);
        write(newsock,buffer,strlen(buffer));
    }    
}

static void handle_shutdown_command(char *src,int newsock,queue *task_queue,FILE *log_file){
    char buffer[1024];
    snprintf(buffer,sizeof(buffer),"[%s] Shutting down manager...\n",timestamp());
    write(newsock,buffer,strlen(buffer));
    snprintf(buffer,sizeof(buffer),"[%s] Waiting for all active workers to finish.\n",timestamp());
    write(newsock,buffer,strlen(buffer));
    pthread_mutex_lock(&task_queue->lockqueue);
    for(int i = task_queue->start; i != task_queue->end; i = (i + 1) % task_queue->max_count){
        QueuedTask *task = &task_queue->data[i];
        if(task->active) task->active = 0;
    }
    pthread_mutex_unlock(&task_queue->lockqueue);
    snprintf(buffer,sizeof(buffer),"[%s] Processing remaining queued tasks.\n",timestamp());
    write(newsock,buffer,strlen(buffer));
    sleep(2); //give it some time
    shutdown_flag = 1;
    pthread_cond_broadcast(&task_queue->not_empty);
    pthread_cond_broadcast(&task_queue->not_full);
    snprintf(buffer,sizeof(buffer),"[%s] Manager shutdown complete.\n",timestamp());
    write(newsock,buffer,strlen(buffer));
    
}

int process_command(char *command,int newsock,queue *task_queues,FILE *log_file){
    char *cmd = strtok(command, " \t\n");
    char *arg1 = strtok(NULL," \t\n");
    char *arg2 = strtok(NULL," \t\n");
    if(!cmd) return -1;
    if(strcmp(cmd,"add") == 0 && arg1 && arg2) handle_add_command(arg1,arg2,newsock,task_queues,log_file);
    else if(strcmp(cmd,"cancel") == 0 && arg1) handle_cancel_command(arg1,newsock,task_queues,log_file);
    else if(strcmp(cmd,"shutdown") == 0) handle_shutdown_command(arg1,newsock,task_queues,log_file);
    else return -1;

    return 0;
}
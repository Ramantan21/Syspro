#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/defines.h"
#include "../include/config_parse.h"
#include "../include/timer.h"
#include "../include/queue.h"
extern void print_queued_task(const QueuedTask *task);
/*
Parsing the config.txt and save all the info to a QueuedTask struct.
What's the approach here: create a *task that points to the current count index of the task
to iterate at send_list.Also update the *task count with the total count of the tasks i have in the config..

*/

int parse_config(FILE *config_file,FILE *log_file,QueuedTask *tasks,int *task_count){
    char line[MAX_LINE];
    int count = 0;

    while(fgets(line,sizeof(line),config_file)){
    line[strcspn(line,"\n")] = '\0';


    QueuedTask *task = &tasks[count]; //a pointer task that points to the count index of the task.
    char *src = strtok(line, " \t");
    char *trg = strtok(NULL, " \t");

    if(!src || !trg){
        fprintf(log_file,"wrong line in config file: %s\n",line);
        continue;
    }  
    char *src_dir = strtok(src,"@");
    char *src_hostport = strtok(NULL,"@");

    if(!src_dir || !src_hostport){
        fprintf(log_file,"wrong source format\n"); //debug
        continue;
    }
    char *src_host = strtok(src_hostport,":");
    char *src_port = strtok(NULL,":");

    if(!src_host || !src_port){
        fprintf(log_file,"wrong source host..\n"); //debug
        continue;
    }

    strncpy(task->source_dir,src_dir,MAX_PATH - 1);
    task->source_dir[MAX_PATH - 1] = '\0';
    strncpy(task->source_host,src_host,MAX_PATH - 1);
    task->source_host[MAX_PATH - 1] = '\0';
    task->source_port = atoi(src_port);

    char *trg_dir = strtok(trg,"@");
    char *trg_hostport = strtok(NULL,"@");
    printf("DEBUG trg_dir is '%s' and trg_hostport is: '%s'\n",trg_dir,trg_hostport);
    char *trg_host = strtok(trg_hostport,":");
    char *trg_port = strtok(NULL,":");
    printf("trg_host is : '%s' and trg_port is: %s\n",trg_host,trg_port);

    strncpy(task->target_dir,trg_dir,MAX_PATH - 1);
    task->target_dir[MAX_PATH - 1] = '\0';
    strncpy(task->target_host,trg_host, MAX_PATH - 1);
    task->target_host[MAX_PATH - 1] = '\0';
    task->target_port = atoi(trg_port);
    task->active = 1;
    
    

    if(count < MAX_SYNC_PAIRS){
        count++; //for each line in config
    }else{
        fprintf(stderr,"too many tasks in config..\n");
        break;
    } //updates task count with the total count of the tasks i've saved
    fprintf(log_file,"[%s] Added file: %s@%s:%d - %s@%s:%d\n",
        timestamp(),
        task->source_dir,task->source_host,task->source_port,
        task->target_dir,task->target_host,task->target_port);
    fflush(log_file);
    }
    *task_count = count; 
    return 0;
}
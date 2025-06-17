#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "../include/defines.h"
#include "../include/queue.h"
#include "../include/process_com.h"
#include "../include/config_parse.h"
#include "../include/thread_pool.h"
#include "../include/timer.h"

void perror_exit(char *message);
void* worker(void *argp);
void send_list(queue *task_queue,QueuedTask *tasks,int task_count);
int send_n_bytes(int sock, char *msg,int n);
int read_n_bytes(int sock,char *buf,int n);
volatile sig_atomic_t shutdown_flag = 0;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex lock gia to manager_logfile
FILE *logfile; 
FILE *config_file;
int default_workers = 5;
queue task_queue;

int main(int argc,char *argv[]){
    if(argc != 11 || strcmp(argv[1],"-l") != 0 || strcmp(argv[3],"-c") != 0 || strcmp(argv[5],"-n") !=0 || strcmp(argv[7],"-p") != 0 || strcmp(argv[9],"-b") != 0){
        fprintf(stderr,"Usage of %s -l manager_logfile -c <config_file> -n <worker_limit> -p <port_number> -b <bufferSize>",argv[0]);
        exit(EXIT_FAILURE);
    }
    int max_workers = atoi(argv[6]);
    if(max_workers <= 0){
        fprintf(stderr,"Invalid worker count %s, using default %d\n",argv[6],default_workers);
        max_workers = default_workers;
    }
    logfile = fopen(argv[2],"w");
    if(logfile == NULL) perror_exit("Error opening logfile");

    config_file = fopen(argv[4],"r");
    if(config_file == NULL) perror_exit("Error opening config file..\n");

    int bufferSize = atoi(argv[10]);
    if(bufferSize <= 0) perror_exit("buffersize invalid\n");

    init_queue(&task_queue,bufferSize);
    QueuedTask tasks[MAX_SYNC_PAIRS];
    int task_count = 0;
    //init thead_pool
    thread_pool_t pool;
    pool.num_threads = max_workers;
    pool.threads = malloc(sizeof(pthread_t) * pool.num_threads);
    //create worker threads
    for(int i = 0; i < pool.num_threads; i++){
        pthread_create(&pool.threads[i],NULL,worker,(void*)&task_queue);
    }
    if(parse_config(config_file,logfile,tasks,&task_count) < 0){
        perror_exit("Parsing config_File failed..\n");
        fclose(config_file);
    }
    send_list(&task_queue,tasks,task_count);
    //connect with nfs_console
    int port,sock,newsock;
    struct sockaddr_in server,client;
    socklen_t clientlen;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct sockaddr *clientptr = (struct sockaddr*)&client;
    port = atoi(argv[8]);
    if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0) perror_exit("socket at connetion with console");
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    if(bind(sock,serverptr,sizeof(server)) < 0) perror_exit("bind at connection with console");
    if(listen(sock,15) < 0) perror_exit("listen");
    while(!shutdown_flag){
        clientlen = sizeof(client);
        if((newsock = accept(sock,clientptr,&clientlen)) < 0) perror_exit("accept connection with console");
        char buffer[BUFFER_SIZE];
        int bytes_read;
        if((bytes_read = read(newsock,buffer,sizeof(buffer) - 1)) < 0) perror_exit("read from console");
        buffer[bytes_read] = '\0';
        char command[MAX_COMMAND];
        strncpy(command,buffer,sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
        if(process_command(command,newsock,&task_queue,logfile) < 0){
            fprintf(logfile,"Failed to process command..\n");
            fflush(logfile);
        }
        close(newsock);
        if(shutdown_flag) break;
    }
    //wait for threads to finish
    for(int i = 0; i < pool.num_threads; i++){
        pthread_join(pool.threads[i],NULL);
    }
    fclose(logfile);
    pthread_mutex_destroy(&task_queue.lockqueue);
    pthread_mutex_destroy(&log_mutex);
    pthread_cond_destroy(&task_queue.not_full);
    pthread_cond_destroy(&task_queue.not_empty);
    free(task_queue.data);
    free(pool.threads);
    close(sock);
}
/*
connects to source_host:source_port that nfs_client is running and sends him LIST
then the client does the rest
the manager reads it  from the buffer and saves the info
***Added an array tasks that contains info from the queuedtask..

*/
void send_list(queue *q,QueuedTask *tasks,int task_count){
        char buffer[BUFFER_SIZE];
        for(int i = 0; i < task_count; i++){
        QueuedTask *task = &tasks[i];
        //connect to nfs_client
        int sock;
        struct sockaddr_in server;
        struct sockaddr *serverptr = (struct sockaddr*)&server;
        struct hostent *rem;
        if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0) perror_exit("socket failed for sending list...\n");
        if((rem = gethostbyname(task->source_host)) == NULL){
            perror("get host by name in source_host failed for sending list  ...\n");
            continue;
        }
        server.sin_family = AF_INET;
        memcpy(&server.sin_addr,rem->h_addr,rem->h_length);
        server.sin_port = htons(task->source_port);
        if(connect(sock,serverptr,sizeof(server)) < 0){
            perror("connecting to client for list failed..\n");
            close(sock);
            continue;
        }
        char command[MAX_COMMAND];
        snprintf(command,sizeof(command),"LIST %s\n",task->source_dir);
        write(sock,command,strlen(command));
        int msg_len, n;
        while((n = read(sock,&msg_len,sizeof(msg_len))) > 0){
            if(read_n_bytes(sock,buffer,msg_len) < 0) break;
            buffer[msg_len] = '\0';
            if(strcmp(buffer,".") == 0) break;
            if(buffer[msg_len - 1] == '\n') buffer[msg_len -1] ='\0';
            QueuedTask new_job;
            strncpy(new_job.source_dir,task->source_dir,MAX_PATH - 1);
            new_job.source_dir[MAX_PATH - 1] = '\0';
            strncpy(new_job.source_host,task->source_host,MAX_PATH -1);
            new_job.source_host[MAX_PATH-1] = '\0';
            new_job.source_port = task->source_port;
            strncpy(new_job.source_file,buffer,MAX_PATH - 1);
            new_job.source_file[MAX_PATH - 1] = '\0';
            strncpy(new_job.target_dir,task->target_dir,MAX_PATH - 1);
            new_job.target_dir[MAX_PATH - 1] = '\0';
            strncpy(new_job.target_host, task->target_host, MAX_PATH - 1);
            new_job.target_host[MAX_PATH - 1] = '\0';
            new_job.target_port = task->target_port;
            strncpy(new_job.target_file, buffer, MAX_PATH - 1);
            new_job.target_file[MAX_PATH - 1] = '\0';
            
            new_job.active = 1;
            // print_queued_task(&new_job);
            push(q,new_job);
        }
        close(sock);
    }
}
/*
a worker thread that handles a job,sends pull /source_dir/file.txt to source nfs_client,
reads back the answear(contains of the file.txt) and then sends push /target_dir/file.txt chunk_size data
to target_host:target_port that nfs_client is connected to

*/

void* worker(void* arg){
    char buffer[BUFFER_SIZE];
    while(!shutdown_flag){
            QueuedTask task = pop(&task_queue); //get a a job
            if(shutdown_flag || strlen(task.source_host) == 0) break;
            if (!task.active) continue; //if the job is not active continue to the other one..
            int err_pull = 0,err_push = 0; //to track results
            int src_sock;
            struct sockaddr_in src_server;
            struct sockaddr *src_serverptr = (struct sockaddr*)&src_server;
            struct hostent *src_rem;
            int bytes_read_total = 0, bytes_written_total = 0;
            if((src_sock = socket(AF_INET,SOCK_STREAM,0)) < 0){
                fprintf(stderr,"creating socket for pull from source failed..\n");
                err_pull = 1;
            }

            if((src_rem = gethostbyname(task.source_host)) == NULL){
                fprintf(stderr,"gethostbyname in source_host for pull failed..\n");
                err_pull = 1;
                close(src_sock);
            }
            src_server.sin_family = AF_INET;
            memcpy(&src_server.sin_addr,src_rem->h_addr,src_rem->h_length);
            src_server.sin_port = htons(task.source_port);

            if(connect(src_sock,src_serverptr,sizeof(src_server)) < 0){
                fprintf(stderr,"Connect to source_port for pull failed..\n");
                err_pull = 1;
            }
            char pull_command[MAX_COMMAND];
            snprintf(pull_command,sizeof(pull_command),"PULL %s/%s\n",task.source_dir,task.source_file);
            if(write(src_sock,pull_command,strlen(pull_command)) < 0){
                err_pull= 1;
                fprintf(stderr,"sending pull failed..\n");
            }
            char filesize_str[32]; //read filesize as string till u find space
            int idx = 0;
            char ch;
            while(read(src_sock,&ch,1) == 1 && ch != ' ' && idx < sizeof(filesize_str) -1){
                filesize_str[idx] = ch;
                idx++;
            }
            filesize_str[idx] = '\0';
            int filesize = atoi(filesize_str);
            //open connection now to target for PUSH
            int trg_sock;
            struct sockaddr_in trg_server;
            struct sockaddr *trg_serverptr = (struct sockaddr*)&trg_server;
            struct hostent *trg_rem;
            if((trg_sock = socket(AF_INET,SOCK_STREAM,0)) < 0){
                fprintf(stderr,"target socket for push commad...\n");
                err_push = 1;
                close(src_sock);
            }
            if((trg_rem = gethostbyname(task.target_host)) == NULL){
                fprintf(stderr,"gethostbyname in push command..\n");
                err_push = 1;
                close(src_sock);
                close(trg_sock);
            }
            trg_server.sin_family = AF_INET;
            memcpy(&trg_server.sin_addr,trg_rem->h_addr,trg_rem->h_length);
            trg_server.sin_port = htons(task.target_port);

            if(connect(trg_sock,trg_serverptr,sizeof(trg_server)) < 0){
                fprintf(stderr,"connect to trg socket for push command..\n");
                err_push = 0;
                close(src_sock);
                close(trg_sock);
            }
            // printf("I'll try to connect to target port now that is: %d..\n",task.target_port);
            int bytes_read = 0;
            //read from src_sock and write to trg_sock
            if(!err_pull && !err_push){
                char push_command[MAX_COMMAND];
                //first send push target_dir file.txt then send the chunk size then the data
                snprintf(push_command,sizeof(push_command),"PUSH %s/%s\n",task.target_dir,task.target_file);
                //send command
                if(send_n_bytes(trg_sock,push_command,strlen(push_command)) < 0){
                    err_push = 1;
                    break;
                }
                // printf("%s\n",push_command);
                if(send_n_bytes(trg_sock, "-1\n", 3) < 0){
                    err_push = 1;
                    break;
                }
                int bytes_left = filesize;
                while(bytes_left > 0){
                    int chunk_size = (bytes_left < CHUNK_SIZE) ? bytes_left : CHUNK_SIZE;
                    //read chunk size bytes from src_sock
                    if(read_n_bytes(src_sock,buffer,chunk_size) < 0){
                        err_pull = 1;
                        break;
                    }
                    char size_str[32]; //send chunksize as string
                    snprintf(size_str,sizeof(size_str),"%d\n",chunk_size);
                    if(send_n_bytes(trg_sock,size_str,strlen(size_str)) < 0){
                        err_push = 1;
                        break;
                    }
                    //send data
                    if(send_n_bytes(trg_sock,buffer,chunk_size) < 0){
                        err_push = 1;
                        break;
                    }
                    bytes_left -= chunk_size;
                    bytes_read_total += chunk_size;
                    bytes_written_total += chunk_size;
            }
            if(send_n_bytes(trg_sock,"0\n",2) < 0) err_push = 1;

        }
            if(bytes_read < 0) err_pull = 1;
            pthread_mutex_lock(&log_mutex);
            if(err_pull == 0){
                fprintf(logfile,"[%s] [%s/%s@%s:%d] [/%s/%s@%s:%d] [%ld] [PULL] [SUCCESS] [Transfered %d bytes]\n",
                        timestamp(),task.source_dir,task.source_file,task.source_host,task.source_port,
                        task.target_dir,task.target_file,task.target_host,task.target_port,
                        pthread_self(),bytes_read_total);
            }else{
                fprintf(logfile,"[%s] [%s/%s@%s:%d] [%s/%s@%s:%d] [%ld] [PULL] [ERROR] [File %s - %s]\n",
                        timestamp(),task.source_dir,task.source_file,task.source_host,task.source_port,
                        task.target_dir,task.target_file,task.target_host,task.target_port,
                        pthread_self(),task.source_file,strerror(errno));
            }
            fflush(logfile);
            pthread_mutex_unlock(&log_mutex);
            pthread_mutex_lock(&log_mutex);
            if(err_push == 0){
                fprintf(logfile,"[%s] [/%s/%s@%s:%d] [%s/%s@%s:%d] [%ld] [PUSH] [SUCESS] [Transfered %d bytes]\n",
                        timestamp(),task.source_dir,task.source_file,task.source_host,task.source_port,
                        task.target_dir,task.target_file,task.target_host,task.target_port,
                        pthread_self(),bytes_written_total);
            }else{
                fprintf(logfile,"[%s] [/%s/%s@%s:%d] [/%s/%s@%s:%d] [%ld] [PUSH] [ERROR] [File %s - %s]\n",
                        timestamp(),task.source_dir,task.source_file,task.source_host,task.source_port,
                        task.target_dir,task.target_file,task.target_host,task.target_port,
                        pthread_self(),task.target_file,strerror(errno)); 
            }
            fflush(logfile);
            pthread_mutex_unlock(&log_mutex);

            close(src_sock);
            close(trg_sock);
        }
    return NULL;
    }
//read bytes from a socket
int read_n_bytes(int sock,char *buf,int n){
    int read_bytes = 0;
    while(read_bytes < n){
        int r = read(sock,buf + read_bytes, n - read_bytes);
        if(r <= 0){
            fprintf(stderr,"Reading from sock failed..\n");
            return -1;
        }
        read_bytes += r;
    }
   return 0;
}
//send bytes to a sock
int send_n_bytes(int sock, char *msg,int n){
    int sent_bytes = 0;
    while(sent_bytes < n){
        int r = write(sock,msg + sent_bytes,n - sent_bytes);
        if(r < 0){
            fprintf(stderr,"Writing to sock failed..\n");
            return -1;
        }
        sent_bytes += r;
    }
    return 0;
}

void perror_exit(char *message){
    perror(message);
    exit(EXIT_FAILURE);
}
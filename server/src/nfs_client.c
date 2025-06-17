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
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include "../include/defines.h"
#include "../include/timer.h"

typedef struct { //struct to store some info
    int client_socket;
    int fd;
    char current_file[MAX_PATH];
}connection_inf;

void perror_exit(char *message);
void handle_pull(int client_socket, const char *file_path);
void handle_list(int client_socket,const char *dir_path);
void handle_push(connection_inf *cnf,const char *file_path,int chunk_size);
void *handle_connection(void *clientsock);

int main(int argc,char *argv[]){
    if(argc!= 3 || strcmp(argv[1],"-p") != 0){
        fprintf(stderr,"Usage of %s -p <port_number>",argv[0]);
        exit(1);
    }
    
    int port = atoi(argv[2]);
    int sock,client_socket;
    struct sockaddr_in server,client;
    socklen_t clientlen = sizeof(client);

    if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0) perror_exit("socket at nfs_client..\n");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    if(bind(sock,(struct sockaddr*)&server,sizeof(server)) < 0) perror_exit("bind in nfs_client for manager..\n");
    

    if(listen(sock,15) < 0) perror_exit("listen in nfs_client..\n");

    // printf("nfs_client listening to port %d\n",port); //debug

    while(1){
        //accept new connection
        client_socket = accept(sock,(struct sockaddr*)&client,&clientlen);
        connection_inf *cnf = malloc(sizeof(connection_inf));
        cnf->client_socket = client_socket;
        cnf->fd = -1;
        cnf->current_file[0] = '\0';

        pthread_t th;
        pthread_create(&th,NULL,handle_connection,cnf);
        pthread_detach(th);
    }
    close(sock);
    return 0;
}

void *handle_connection(void *arg){
    connection_inf *cnf = (connection_inf*)arg;
    char buffer[BUFFER_SIZE];
    while(1){
        int idx = 0;
        char ch;
        while(read(cnf->client_socket,&ch,1) == 1){
            buffer[idx++] = ch;
            if(ch == '\n' || idx == sizeof(buffer) - 1) break;
        }
        if(idx == 0) break;
        buffer[idx] = '\0';
        char *saveptr;
        char *cmd = strtok_r(buffer," \t\n",&saveptr); //break command with strtok_r cause it's thread safe..
        char *arg1 = strtok_r(NULL, "\t\n",&saveptr);

        if(cmd && strcmp(cmd,"LIST") == 0 && arg1) handle_list(cnf->client_socket,arg1);
        if(cmd && strcmp(cmd, "PULL") == 0 && arg1) handle_pull(cnf->client_socket,arg1);
        if(cmd && strcmp(cmd,"PUSH") == 0 && arg1) handle_push(cnf,arg1,0);
    }

    if(cnf->fd != -1) close(cnf->fd);
    close(cnf->client_socket);
    free(cnf);
    return NULL;
}

void handle_list(int client_socket,const char *dir_path){
    DIR *dir = opendir(dir_path);
    if(dir == NULL){
        fprintf(stderr,"Failed opening source_dir at handle_list\n");
        return;
    }
    char buffer[MAX_FILENAME + 2]; // +2 gia newline kai eof
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name,".") != 0 && strcmp(entry->d_name,"..")){
            if(strlen(entry->d_name) >= sizeof(buffer) - 2){
                fprintf(stderr,"filename too big..\n");
                continue;
            }
            snprintf(buffer,sizeof(buffer),"%s\n",entry->d_name);
            int msg_len = strlen(buffer);
            write(client_socket,&msg_len,sizeof(int));
            write(client_socket,buffer,msg_len);
        }
    }
    int end_len = 1;
    write(client_socket,&end_len,sizeof(int));
    write(client_socket,".",1); //end of list
    closedir(dir);
}

void handle_pull(int client_socket,const char *file_path){
    char buffer[BUFFER_SIZE];
    int fd = open(file_path,O_RDONLY); 
    if(fd < 0){
        write(client_socket,"-1\n",3);
        return;
    }
    struct stat st;
    off_t filesize = 0;
    if(fstat(fd,&st) == 0){
        filesize = st.st_size;
    }
    char message[64];
    int message_len = snprintf(message,sizeof(message),"%ld ",(long)filesize); //send filesize with space
    write(client_socket,message,message_len);
    ssize_t bytes_read;
    while((bytes_read = read(fd,buffer,sizeof(buffer))) > 0){
        write(client_socket,buffer,bytes_read);
    }
    close(fd);
}

void handle_push(connection_inf *cnf,const char *file_path, int chunk_size){
    while(1){
        char sizebuf[32]; //read chunk_size as string
        int idx = 0;
        char ch;
        while(recv(cnf->client_socket,&ch,1,0) == 1){
            sizebuf[idx++] = ch;
            if(ch == '\n' || idx == sizeof(sizebuf) - 1) break;
        }
        sizebuf[idx] = '\0';
        int chunk_size = atoi(sizebuf);
        if(chunk_size == -1){ //if == -1 then then file is written from scratch
            if(cnf->fd != -1) close(cnf->fd);
            cnf->fd = open(file_path,O_WRONLY | O_CREAT | O_TRUNC, 0666);
            strncpy(cnf->current_file,file_path,MAX_PATH);
        }else if (chunk_size > 0){
            if(cnf->fd == -1){
                cnf->fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
                strncpy(cnf->current_file,file_path,MAX_PATH);
            }
            char buffer[BUFFER_SIZE];
            int bytes_left = chunk_size;
            while(bytes_left > 0){
                int to_read = bytes_left < sizeof(buffer) ? bytes_left : sizeof(buffer);
                int n = read(cnf->client_socket,buffer,to_read);
                if( n <= 0 ) break;
                write(cnf->fd,buffer,n);
                bytes_left -= n;
            }
        }else if (chunk_size == 0){
            //end of transfering data
            if(cnf->fd!= -1){
                close(cnf->fd);
                cnf->fd = -1;
            }
            break;
        }
    }
}

void perror_exit(char *message){
    perror(message);
    exit(EXIT_FAILURE);
}
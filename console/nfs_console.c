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
#include "../include/defines.h"

void perror_exit(char *message);


int main(int argc,char *argv[]){
    if(argc != 7 || strcmp(argv[1],"-l") != 0 || strcmp(argv[3],"-h") != 0 || strcmp(argv[5],"-p") != 0){
        fprintf(stderr,"Usage of %s -l <console_logfile> -h <host_IP> -p <port_number>",argv[0]);
        exit(1);
    }
    char *console_logfile = argv[2];
    char *host_ip = argv[4];
    FILE *log_file = fopen(console_logfile,"w");
    if(!log_file) perror_exit("Failed to open console_logfile");
    char command[MAX_COMMAND];
    while(1){
        printf("> ");
        fflush(stdout);
        if(fgets(command,MAX_COMMAND,stdin) == NULL) break;
        size_t len = strlen(command);
        if(len > 0 && command[len - 1] == '\n'){
            command[len - 1] = '\0';
        }
        if(strlen(command) == 0) continue;
        if(strcmp(command,"exit") == 0) break;
        fprintf(log_file,"Command %s\n",command);
        int port,sock;
        char buf[MAX_COMMAND];
        struct sockaddr_in server;
        struct sockaddr *serverptr = (struct sockaddr*)&server;
        struct hostent *rem;
        if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0) perror_exit("socket");
        if((rem = gethostbyname(host_ip)) == NULL){
            perror_exit("get hostbyname in connection with manager..\n");
        }
        port = atoi(argv[6]);
        server.sin_family = AF_INET;
        memcpy(&server.sin_addr,rem->h_addr,rem->h_length);
        server.sin_port = htons(port);
        if(connect(sock,serverptr,sizeof(server)) < 0) perror_exit("connect");
        write(sock,command,strlen(command));
        write(sock,"\n",1);

        int bytes_read;
        while((bytes_read = read(sock,buf,sizeof(buf))) > 0){
            buf[bytes_read] = '\0';
            printf("%s",buf);
            fprintf(log_file,"%s",buf);
            fflush(log_file);
        }
        close(sock);
        if(strcmp(command,"shutdown") == 0){
            close(sock);
            exit(0);
        }
    }
}

void perror_exit(char *message){
    perror(message);
    exit(EXIT_FAILURE);
}
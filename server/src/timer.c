#include "../include/timer.h"
#include <stdio.h>
#include <time.h>

char *timestamp(void){
    static char buffer[32];
    time_t timer = time(NULL);
    struct tm *tm_info = localtime(&timer);
    strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",tm_info);
    return buffer;
}

void log_message(FILE *log_file,const char *format){
    fprintf(log_file,"%s %s\n",timestamp(),format);
    fflush(log_file);
}
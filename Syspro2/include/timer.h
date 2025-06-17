#ifndef TIMER_H
#define TIMER_H

#include <stdio.h>
#include <time.h>

char *timestamp(void);

void lost_message(FILE *log_file,const char *format);

#endif
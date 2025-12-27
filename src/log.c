#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

void log_info(const char *fmt, ...){
    FILE *f=fopen("logs/server.log","a");
    if(!f) return;
    time_t t=time(NULL);
    struct tm *tm=localtime(&t);

    fprintf(f,"[%04d-%02d-%02d %02d:%02d:%02d] ",
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);

    va_list ap; va_start(ap,fmt);
    vfprintf(f,fmt,ap);
    va_end(ap);

    fprintf(f,"\n");
    fclose(f);
}

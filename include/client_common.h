#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define CBUF 4096

// đọc response cho tới khi gặp dòng trống
static int recv_response(int sock, char *out, int maxlen){
    int len = 0;
    out[0] = 0;

    while(len < maxlen-1){
        char ch;
        int n = recv(sock, &ch, 1, 0);
        if(n <= 0) return 0;
        out[len++] = ch;
        out[len] = 0;

        // kết thúc khi gặp \n\n hoặc \r\n\r\n
        if(len >= 2 && out[len-1]=='\n' && out[len-2]=='\n') break;
        if(len >= 4 && out[len-1]=='\n' && out[len-2]=='\r' &&
                       out[len-3]=='\n' && out[len-4]=='\r') break;
    }
    return 1;
}

static void trim_newline(char *s){
    int n=strlen(s);
    while(n>0 && (s[n-1]=='\n' || s[n-1]=='\r')) s[--n]=0;
}

static void input_line(const char *prompt, char *buf, int maxlen){
    printf("%s", prompt);
    fflush(stdout);
    if(fgets(buf, maxlen, stdin)==NULL) buf[0]=0;
    trim_newline(buf);
}

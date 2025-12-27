#include "protocol.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

int try_extract_message(Client *c, char *out_msg) {
    c->inbuf[c->inlen] = '\0';

    char *p = strstr(c->inbuf, "\r\n\r\n");
    if (!p) return 0;

    int delim_len = 4;
    int msg_len = (int)(p - c->inbuf) + delim_len;

    memcpy(out_msg, c->inbuf, msg_len);
    out_msg[msg_len] = '\0';

    int remain = c->inlen - msg_len;
    memmove(c->inbuf, c->inbuf + msg_len, remain);
    c->inlen = remain;
    return 1;
}

static void trim(char *s){
    while(*s==' '||*s=='\t') memmove(s,s+1,strlen(s));
    int n=strlen(s);
    while(n>0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) s[--n]=0;
}

int parse_command(const char *msg, char *out_cmd){
    
    const char *end = strchr(msg, '\n');
    int len = end ? (end - msg) : (int)strlen(msg);

    char firstline[128];
    if(len >= (int)sizeof(firstline)) len = sizeof(firstline)-1;
    memcpy(firstline, msg, len);
    firstline[len] = 0;
    trim(firstline);

    
    if(sscanf(firstline, "CMD %31s", out_cmd) != 1) return 0;
    return 1;
}

const char* find_field(const char *msg, const char *key, char *out_val, int maxlen){
   
    const char *p = msg;
    int keylen = strlen(key);

    while(*p){
        
        const char *line_end = strchr(p, '\n');
        int linelen = line_end ? (line_end - p) : (int)strlen(p);

        if(linelen <= 1){ 
            break;
        }

        
        char line[512];
        int cplen = linelen;
        if(cplen >= (int)sizeof(line)) cplen = sizeof(line)-1;
        memcpy(line, p, cplen);
        line[cplen] = 0;
        trim(line);

        
        if(strncasecmp(line, key, keylen)==0 && line[keylen]==':'){
            char *val = line + keylen + 1; 
            trim(val);
            strncpy(out_val, val, maxlen-1);
            out_val[maxlen-1]=0;
            return out_val;
        }

        if(!line_end) break;
        p = line_end + 1;
    }
    return NULL;
}

void build_response(char *out, int code, const char *message, const char *extra) {
    if (extra && strlen(extra)>0)
        sprintf(out, "RES %d %s\r\n%s\r\n\r\n", code, message, extra);
    else
        sprintf(out, "RES %d %s\r\n\r\n", code, message);
}

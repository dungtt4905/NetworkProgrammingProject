#include "protocol.h"

int try_extract_message(Client *c, char *out_msg) {
    c->inbuf[c->inlen] = '\0';

    char *p = strstr(c->inbuf, "\r\n\r\n");
    int delim_len = 4;
    if (!p) {
        p = strstr(c->inbuf, "\n\n");
        delim_len = 2;
    }
    if (!p) return 0;

    int msg_len = p - c->inbuf + delim_len;
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

int parse_request(const char *msg, Request *req) {
    memset(req, 0, sizeof(*req));

    char buf[MSG_MAX];
    strncpy(buf, msg, MSG_MAX-1);

    char *line = strtok(buf, "\n");
    if (!line) return 0;

    if (sscanf(line, "CMD %31s", req->cmd) != 1) return 0;

    while ((line = strtok(NULL, "\r\n")) != NULL) {
        if (strlen(line)==0) break;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = 0;
        char *key = line;
        char *val = colon+1;
        trim(key); trim(val);
        strncpy(req->keys[req->count], key, 63);
        strncpy(req->values[req->count], val, 255);
        req->count++;
    }
    return 1;
}

void build_response(char *out, int code, const char *message, const char *extra) {
    if (extra && strlen(extra)>0)
        sprintf(out, "RES %d %s\r\n%s\r\n\r\n", code, message, extra);
    else
        sprintf(out, "RES %d %s\r\n\r\n", code, message);
}


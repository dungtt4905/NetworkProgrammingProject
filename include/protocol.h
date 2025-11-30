#pragma once
#include "common.h"

typedef struct {
    char cmd[32];
    char keys[32][64];
    char values[32][256];
    int count;
} Request;

int try_extract_message(Client *c, char *out_msg);
int parse_request(const char *msg, Request *req);
void build_response(char *out, int code, const char *message, const char *extra);


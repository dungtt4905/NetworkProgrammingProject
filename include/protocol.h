#pragma once
#include "common.h"

int parse_command(const char *msg, char *out_cmd); 
const char* find_field(const char *msg, const char *key, char *out_val, int maxlen);
void build_response(char *out, int code, const char *message, const char *extra);
int try_extract_message(Client *c, char *out_msg);

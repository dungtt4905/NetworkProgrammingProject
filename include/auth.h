#pragma once
#include "common.h"
#include "protocol.h"

int handle_register(Client *c, const char *msg, char *res);
int handle_login(Client *c, const char *msg, char *res);
int handle_logout(Client *c, const char *msg, char *res);

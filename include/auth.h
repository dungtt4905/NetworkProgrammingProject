#pragma once
#include "common.h"
#include "protocol.h"

int handle_register(Client *c, Request *req, char *res);
int handle_login(Client *c, Request *req, char *res);
int handle_logout(Client *c, Request *req, char *res);


#pragma once
#include "common.h"

int handle_list_projects(Client *c, const char *msg, char *res);
int handle_search_project(Client *c, const char *msg, char *res);
int handle_create_project(Client *c, const char *msg, char *res);
int handle_add_member(Client *c, const char *msg, char *res);

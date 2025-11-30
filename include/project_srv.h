#pragma once
#include "common.h"
#include "protocol.h"

int handle_list_projects(Client *, Request *, char *);
int handle_search_project(Client *, Request *, char *);
int handle_create_project(Client *, Request *, char *);
int handle_add_member(Client *, Request *, char *);
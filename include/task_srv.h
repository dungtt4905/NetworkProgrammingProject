#pragma once
#include "common.h"

int handle_list_tasks(Client *c, const char *msg, char *res);
int handle_create_task(Client *c, const char *msg, char *res);
int handle_assign_task(Client *c, const char *msg, char *res);
int handle_update_task_status(Client *c, const char *msg, char *res);

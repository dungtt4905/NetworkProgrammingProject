#pragma once
#include "common.h"

Role get_role_in_project(int user_id, int project_id);
int require_login(Client *c, char *res); 
int require_role(Client *c, int project_id, Role min_role, int fail_code, char *res);


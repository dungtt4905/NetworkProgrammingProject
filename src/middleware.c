#include "middleware.h"
#include "protocol.h"
#include "storage.h"

int require_login(Client *c, char *res){
    if (c->user_id < 0){
        build_response(res, 116, "Not logged in", NULL);
        return 1;
    }
    return 0;
}

int require_role(Client *c, int project_id, Role min_role, int fail_code, char *res){
    Role r = get_role_in_project(c->user_id, project_id);
    if (r == ROLE_OUTSIDER){
        build_response(res, 214, "Not a member of project", NULL);
        return 1;
    }
    if (r > min_role){ 
        build_response(res, fail_code, "Permission denied", NULL);
        return 1;
    }
    return 0;
}
Role get_role_in_project(int user_id, int project_id){
    Membership *m = find_membership(project_id, user_id);
    if(!m) return ROLE_OUTSIDER;
    return m->role;
}
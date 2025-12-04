#include "project_srv.h"
#include "storage.h"
#include "protocol.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int valid_project_name(const char *s){
    if(!s) return 0;
    int n = (int)strlen(s);
    return n >= 3 && n <= 63;
}

int handle_list_projects(Client *c, const char *msg, char *res){
    (void)msg;

    char extra[MSG_MAX];

    int pos = 0, count = 0;

    // đếm + in danh sách dự án user tham gia
    for(int i=0;i<members_count;i++){
        if(members[i].user_id == c->user_id){
            Project *p = find_project_by_id(members[i].project_id);
            if(!p) continue;

            const char *role_str =
                (members[i].role==ROLE_OWNER) ? "OWNER" : "MEMBER";

            // in từng dòng dự án
            pos += snprintf(extra+pos, sizeof(extra)-pos,
                            "P%d: %d|%s|%s\r\n",
                            count+1, p->id, p->name, role_str);
            count++;
            if(pos >= (int)sizeof(extra)) break; // tránh overflow
        }
    }

    // đẩy header Count lên đầu bằng cách tạo buffer mới
    char final_extra[MSG_MAX];
    int pos2 = 0;

    pos2 += snprintf(final_extra+pos2, sizeof(final_extra)-pos2,
                     "Count: %d\r\n", count);
    pos2 += snprintf(final_extra+pos2, sizeof(final_extra)-pos2,
                     "%s", extra);

    build_response(res, 200, "List projects success", final_extra);
    return 0;
}

int handle_search_project(Client *c, const char *msg, char *res){
    (void)c;

    char kw[128];
    if(!find_field(msg, "Keyword", kw, sizeof(kw)) || strlen(kw)==0){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    char listbuf[MSG_MAX];
    char extra[MSG_MAX];

    int pos = 0, count = 0;

    for(int i=0;i<projects_count;i++){
        if(strstr(projects[i].name, kw)){
            User *owner = find_user_by_id(projects[i].owner_id);
            const char *owner_name = owner ? owner->username : "unknown";

            pos += snprintf(listbuf+pos, sizeof(listbuf)-pos,
                            "P%d: %d|%s|%s\r\n",
                            count+1, projects[i].id, projects[i].name, owner_name);
            count++;
            if(pos >= (int)sizeof(listbuf)) break;
        }
    }

    int pos2 = 0;
    pos2 += snprintf(extra+pos2, sizeof(extra)-pos2,
                     "Count: %d\r\n", count);
    pos2 += snprintf(extra+pos2, sizeof(extra)-pos2,
                     "%s", listbuf);

    build_response(res, 200, "Search projects success", extra);
    return 0;
}

int handle_create_project(Client *c, const char *msg, char *res){
    char name[128];

    if(!find_field(msg,"Name",name,sizeof(name))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }
    if(!valid_project_name(name)){
        build_response(res, 422, "Project name invalid", NULL);
        return 0;
    }

    // cấm trùng tên để thao tác theo tên được duy nhất
    if(find_project_by_name(name)){
        build_response(res, 409, "Project name already exists", NULL);
        return 0;
    }

    if(projects_count >= MAX_PROJECTS){
        build_response(res, 507, "Storage full", NULL);
        return 0;
    }

    Project p; memset(&p,0,sizeof(p));
    p.id = next_project_id();
    strncpy(p.name, name, sizeof(p.name)-1);
    p.name[sizeof(p.name)-1] = 0;
    p.owner_id = c->user_id;

    projects[projects_count++] = p;
    storage_save_projects();

    // auto add owner membership
    if(members_count < MAX_MEMBERS){
        Membership m; memset(&m,0,sizeof(m));
        m.project_id = p.id;
        m.user_id = c->user_id;
        m.role = ROLE_OWNER;
        members[members_count++] = m;
        storage_save_members();
    }

    char extra[64];
    snprintf(extra,sizeof(extra),"ProjectId: %d", p.id);
    build_response(res, 201, "Create project success", extra);
    log_info("CREATE_PROJECT id=%d name=%s owner=%d",
             p.id, p.name, p.owner_id);
    return 0;
}

int handle_add_member(Client *c, const char *msg, char *res){
    char pname[128], uname[64];

    if(!find_field(msg,"ProjectName",pname,sizeof(pname)) ||
       !find_field(msg,"Username",uname,sizeof(uname))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    Project *p = find_project_by_name(pname);
    if(!p){
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }

    // nếu bạn vẫn muốn phòng trùng hiếm:
    if(count_project_by_name(pname) > 1){
        build_response(res, 409, "Project name ambiguous", NULL);
        return 0;
    }

    int pid = p->id;

    // owner-only
    Membership *me = find_membership(pid, c->user_id);
    if(!me || me->role != ROLE_OWNER){
        build_response(res, 403, "Forbidden (owner only)", NULL);
        return 0;
    }

    User *u = find_user_by_username(uname);
    if(!u){
        build_response(res, 404, "User not found", NULL);
        return 0;
    }

    if(find_membership(pid, u->id)){
        build_response(res, 409, "Already a member", NULL);
        return 0;
    }

    if(members_count >= MAX_MEMBERS){
        build_response(res, 507, "Storage full", NULL);
        return 0;
    }

    Membership m; memset(&m,0,sizeof(m));
    m.project_id = pid;
    m.user_id = u->id;
    m.role = ROLE_MEMBER;
    members[members_count++] = m;
    storage_save_members();

    build_response(res, 200, "Add member success", NULL);
    log_info("ADD_MEMBER project=%d user=%d by_owner=%d",
             pid, u->id, c->user_id);
    return 0;
}

#include "task_srv.h"
#include "storage.h"
#include "protocol.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int parse_status(const char *s, TaskStatus *out){
    if(!s) return 0;
    if(strcmp(s,"TODO")==0) *out=TODO;
    else if(strcmp(s,"IN_PROGRESS")==0) *out=IN_PROGRESS;
    else if(strcmp(s,"DONE")==0) *out=DONE;
    else return 0;
    return 1;
}

int handle_list_tasks(Client *c, const char *msg, char *res){
    char pname[128];

    if(!find_field(msg,"ProjectName",pname,sizeof(pname))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    Project *p = find_project_by_name(pname);
    if(!p){
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }
    if(count_project_by_name(pname) > 1){
        build_response(res, 409, "Project name ambiguous", NULL);
        return 0;
    }

    int pid = p->id;

    // must be member
    if(!find_membership(pid, c->user_id)){
        build_response(res, 403, "Forbidden (not member)", NULL);
        return 0;
    }

    char listbuf[MSG_MAX];
    listbuf[0] = 0;                 // FIX: init rỗng
    int pos = 0, count = 0;

    for(int i=0;i<tasks_count;i++){
        if(tasks[i].project_id==pid){
            User *ass = find_user_by_id(tasks[i].assignee_id);
            const char *ass_name = ass ? ass->username : "unassigned";

            pos += snprintf(listbuf+pos, sizeof(listbuf)-pos,
                            "T%d: %d|%s|%s|%d\r\n",
                            count+1, tasks[i].id, tasks[i].title, ass_name, tasks[i].status);
            count++;
            if(pos >= (int)sizeof(listbuf)) break;
        }
    }

    char extra[MSG_MAX];
    int pos2 = 0;
    pos2 += snprintf(extra+pos2, sizeof(extra)-pos2,
                     "Count: %d\r\n", count);
    pos2 += snprintf(extra+pos2, sizeof(extra)-pos2,
                     "%s", listbuf);

    build_response(res, 200, "List tasks success", extra);
    return 0;
}

int handle_create_task(Client *c, const char *msg, char *res){
    char pname[128], title[128], desc[256];

    // chỉ cần ProjectName, Title, Desc
    if(!find_field(msg,"ProjectName",pname,sizeof(pname)) ||
       !find_field(msg,"Title",title,sizeof(title)) ||
       !find_field(msg,"Desc",desc,sizeof(desc))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    Project *p = find_project_by_name(pname);
    if(!p){
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }
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

    if(strlen(title)<3 || strlen(title)>63){
        build_response(res, 422, "Task title invalid", NULL);
        return 0;
    }

    if(tasks_count >= MAX_TASKS){
        build_response(res, 507, "Storage full", NULL);
        return 0;
    }

    Task t; memset(&t,0,sizeof(t));
    t.id = next_task_id();
    t.project_id = pid;
    strncpy(t.title,title,sizeof(t.title)-1);
    t.title[sizeof(t.title)-1] = 0;
    strncpy(t.desc,desc,sizeof(t.desc)-1);
    t.desc[sizeof(t.desc)-1] = 0;

    t.assignee_id = -1;  // CHƯA GÁN AI
    t.status = TODO;

    tasks[tasks_count++] = t;
    storage_save_tasks();

    char extra[64];
    snprintf(extra,sizeof(extra),"TaskId: %d", t.id);
    build_response(res, 201, "Create task success", extra);

    log_info("CREATE_TASK id=%d project=%d by=%d",
             t.id, pid, c->user_id);
    return 0;
}

int handle_assign_task(Client *c, const char *msg, char *res){
    char pname[128], title[128], ass_u[64];

    // ASSIGN theo ProjectName + TaskTitle + Assignee
    if(!find_field(msg,"ProjectName",pname,sizeof(pname)) ||
       !find_field(msg,"TaskTitle",title,sizeof(title)) ||
       !find_field(msg,"Assignee",ass_u,sizeof(ass_u))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    Project *p = find_project_by_name(pname);
    if(!p){
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }
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

    Task *t = find_task_by_title(pid, title);
    if(!t){
        build_response(res, 404, "Task not found", NULL);
        return 0;
    }
    if(count_task_by_title(pid, title) > 1){
        build_response(res, 409, "Task title ambiguous", NULL);
        return 0;
    }

    User *assignee = find_user_by_username(ass_u);
    if(!assignee){
        build_response(res, 404, "Assignee not found", NULL);
        return 0;
    }
    if(!find_membership(pid, assignee->id)){
        build_response(res, 403, "Forbidden (assignee not member)", NULL);
        return 0;
    }

    t->assignee_id = assignee->id;
    storage_save_tasks();

    build_response(res, 200, "Assign task success", NULL);
    log_info("ASSIGN_TASK task=%d(title=%s) assignee=%d by=%d",
             t->id, t->title, assignee->id, c->user_id);
    return 0;
}

int handle_update_task_status(Client *c, const char *msg, char *res){
    char pname[128], title[128], st_s[32];

    // UPDATE theo ProjectName + TaskTitle + Status
    if(!find_field(msg,"ProjectName",pname,sizeof(pname)) ||
       !find_field(msg,"TaskTitle",title,sizeof(title)) ||
       !find_field(msg,"Status",st_s,sizeof(st_s))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    Project *p = find_project_by_name(pname);
    if(!p){
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }
    if(count_project_by_name(pname) > 1){
        build_response(res, 409, "Project name ambiguous", NULL);
        return 0;
    }
    int pid = p->id;

    Membership *me = find_membership(pid, c->user_id);
    if(!me){
        build_response(res, 403, "Forbidden (not member)", NULL);
        return 0;
    }

    Task *t = find_task_by_title(pid, title);
    if(!t){
        build_response(res, 404, "Task not found", NULL);
        return 0;
    }
    if(count_task_by_title(pid, title) > 1){
        build_response(res, 409, "Task title ambiguous", NULL);
        return 0;
    }

    TaskStatus newst;
    if(!parse_status(st_s, &newst)){
        build_response(res, 422, "Status invalid", NULL);
        return 0;
    }

    // Quyền update:
    // - OWNER luôn được
    // - MEMBER chỉ được nếu là assignee
    if(me->role == ROLE_MEMBER && t->assignee_id != c->user_id){
        build_response(res, 403, "Forbidden (not assignee)", NULL);
        return 0;
    }

    if(t->status == DONE && newst == DONE){
        build_response(res, 409, "Task already completed", NULL);
        return 0;
    }

    t->status = newst;
    storage_save_tasks();

    build_response(res, 200, "Update task status success", NULL);
    log_info("UPDATE_TASK_STATUS task=%d(title=%s) status=%d by=%d",
             t->id, t->title, newst, c->user_id);
    return 0;
}

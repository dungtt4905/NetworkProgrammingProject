#include "comment_srv.h"
#include "storage.h"
#include "protocol.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int handle_comment_task(Client *c, const char *msg, char *res){
    char pid_str[32], tid_str[32], comment[256];

    if(!find_field(msg,"ProjectId",pid_str,sizeof(pid_str)) ||
       !find_field(msg,"TaskId",tid_str,sizeof(tid_str)) ||
       !find_field(msg,"Comment",comment,sizeof(comment))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }
    if(strlen(comment)==0){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    int pid = atoi(pid_str);
    int tid = atoi(tid_str);
    
    if(pid <= 0 || tid <= 0){
        build_response(res, 400, "Invalid ID", NULL);
        return 0;
    }

    Project *p = find_project_by_id(pid);
    if(!p){
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }

    Membership *me = find_membership(pid, c->user_id);
    if(!me){
        build_response(res, 403, "Forbidden (not member)", NULL);
        return 0;
    }

    Task *t = find_task_by_id(tid);
    if(!t || t->project_id != pid){
        build_response(res, 404, "Task not found", NULL);
        return 0;
    }

    if(comments_count >= MAX_COMMENTS){
        build_response(res, 507, "Storage full", NULL);
        return 0;
    }

    Comment cm; memset(&cm,0,sizeof(cm));
    cm.id        = next_comment_id();
    cm.task_id   = t->id;
    cm.author_id = c->user_id;
    strncpy(cm.content, comment, sizeof(cm.content)-1);
    cm.content[sizeof(cm.content)-1] = 0;
    cm.created_at = time(NULL);

    comments[comments_count++] = cm;
    storage_save_comments();

    build_response(res, 200, "Comment success", NULL);
    log_info("COMMENT taskId=%d(title=%s) authorId=%d projectId=%d",
             t->id, t->title, c->user_id, pid);
    return 0;
}

#include "comment_srv.h"
#include "storage.h"
#include "protocol.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int handle_comment_task(Client *c, const char *msg, char *res){
    char pname[128], title[128], content[256];

    // cần đủ 3 field
    if(!find_field(msg,"ProjectName",pname,sizeof(pname)) ||
       !find_field(msg,"TaskTitle",title,sizeof(title)) ||
       !find_field(msg,"Content",content,sizeof(content))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }
    if(strlen(content)==0){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    // 1) tìm project theo tên
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

    // 2) phải là người trong dự án (member/owner đều ok)
    Membership *me = find_membership(pid, c->user_id);
    if(!me){
        build_response(res, 403, "Forbidden (not member)", NULL);
        return 0;
    }

    // 3) tìm task theo title trong project đó
    Task *t = find_task_by_title(pid, title);
    if(!t){
        build_response(res, 404, "Task not found", NULL);
        return 0;
    }
    if(count_task_by_title(pid, title) > 1){
        build_response(res, 409, "Task title ambiguous", NULL);
        return 0;
    }

    // 4) lưu comment (không cần check DONE/assignee)
    if(comments_count >= MAX_COMMENTS){
        build_response(res, 507, "Storage full", NULL);
        return 0;
    }

    Comment cm; memset(&cm,0,sizeof(cm));
    cm.id        = next_comment_id();
    cm.task_id   = t->id;
    cm.author_id = c->user_id;
    strncpy(cm.content, content, sizeof(cm.content)-1);
    cm.content[sizeof(cm.content)-1] = 0;
    cm.created_at = time(NULL);

    comments[comments_count++] = cm;
    storage_save_comments();

    build_response(res, 200, "Comment success", NULL);
    log_info("COMMENT task=%d(title=%s) author=%d project=%d",
             t->id, t->title, c->user_id, pid);
    return 0;
}

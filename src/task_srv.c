#include "task_srv.h"
#include "storage.h"
#include "protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int parse_status(const char *s, TaskStatus *out)
{
    if (!s)
        return 0;
    if (strcmp(s, "TODO") == 0)
        *out = TODO;
    else if (strcmp(s, "IN_PROGRESS") == 0)
        *out = IN_PROGRESS;
    else if (strcmp(s, "DONE") == 0)
        *out = DONE;
    else
        return 0;
    return 1;
}

int handle_list_tasks(Client *c, const char *msg, char *res)
{
    char pid_str[32];

    if (!find_field(msg, "ProjectId", pid_str, sizeof(pid_str)))
    {
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    int pid = atoi(pid_str);
    if (pid <= 0)
    {
        build_response(res, 400, "Invalid project ID", NULL);
        return 0;
    }

    Project *p = find_project_by_id(pid);
    if (!p)
    {
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }

    Membership *me = find_membership(pid, c->user_id);
    if (!me)
    {
        build_response(res, 403, "Forbidden (not member)", NULL);
        return 0;
    }

    char listbuf[MSG_MAX];
    listbuf[0] = 0;
    int pos = 0, count = 0;

    for (int i = 0; i < tasks_count; i++)
    {
        if (tasks[i].project_id == pid)
        {
            User *ass = find_user_by_id(tasks[i].assignee_id);
            const char *ass_name = ass ? ass->username : "unassigned";

            pos += snprintf(listbuf + pos, sizeof(listbuf) - pos, "T%d: %d|%s|%s|%d\r\n",
                            count + 1, tasks[i].id, tasks[i].title, ass_name, tasks[i].status);
            count++;
            if (pos >= (int)sizeof(listbuf))
                break;
        }
    }

    char extra[MSG_MAX];
    int pos2 = 0;
    pos2 += snprintf(extra + pos2, sizeof(extra) - pos2,
                     "TaskCount: %d\r\n", count);
    pos2 += snprintf(extra + pos2, sizeof(extra) - pos2,
                     "%s", listbuf);

    build_response(res, 200, "List tasks success", extra);
    log_info("LIST_TASKS projectId=%d userId=%d", pid, c->user_id);
    return 0;
}

int handle_create_task(Client *c, const char *msg, char *res)
{
    char pid_str[32], title[128], desc[256];

    if (!find_field(msg, "ProjectId", pid_str, sizeof(pid_str)) ||
        !find_field(msg, "Title", title, sizeof(title)) ||
        !find_field(msg, "Desc", desc, sizeof(desc)))
    {
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    int pid = atoi(pid_str);
    if (pid <= 0)
    {
        build_response(res, 400, "Invalid project ID", NULL);
        return 0;
    }

    Project *p = find_project_by_id(pid);
    if (!p)
    {
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }

    Membership *me = find_membership(pid, c->user_id);
    if (!me)
    {
        build_response(res, 403, "Forbidden (not member)", NULL);
        return 0;
    }

    if (me->role != ROLE_OWNER)
    {
        build_response(res, 403, "Forbidden (owner only)", NULL);
        return 0;
    }

    if (strlen(title) < 3 || strlen(title) > 63)
    {
        build_response(res, 422, "Task title invalid", NULL);
        return 0;
    }

    if (tasks_count >= MAX_TASKS)
    {
        build_response(res, 507, "Storage full", NULL);
        return 0;
    }

    Task t;
    memset(&t, 0, sizeof(t));
    t.id = next_task_id();
    t.project_id = pid;
    strncpy(t.title, title, sizeof(t.title) - 1);
    t.title[sizeof(t.title) - 1] = 0;
    strncpy(t.desc, desc, sizeof(t.desc) - 1);
    t.desc[sizeof(t.desc) - 1] = 0;

    t.assignee_id = -1;
    t.status = TODO;

    tasks[tasks_count++] = t;
    storage_save_tasks();

    char extra[64];
    snprintf(extra, sizeof(extra), "TaskId: %d", t.id);
    build_response(res, 201, "Create task success", extra);

    log_info("CREATE_TASK taskId=%d projectId=%d byUserId=%d",
             t.id, pid, c->user_id);
    return 0;
}

int handle_assign_task(Client *c, const char *msg, char *res)
{
    char pid_str[32], tid_str[32], assignee_id_str[32];

    if (!find_field(msg, "ProjectId", pid_str, sizeof(pid_str)) ||
        !find_field(msg, "TaskId", tid_str, sizeof(tid_str)) ||
        !find_field(msg, "AssigneeId", assignee_id_str, sizeof(assignee_id_str)))
    {
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    int pid = atoi(pid_str);
    int tid = atoi(tid_str);
    int assignee_id = atoi(assignee_id_str);

    if (pid <= 0 || tid <= 0 || assignee_id <= 0)
    {
        build_response(res, 400, "Invalid ID", NULL);
        return 0;
    }

    Project *p = find_project_by_id(pid);
    if (!p)
    {
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }

    Membership *me = find_membership(pid, c->user_id);
    if (!me)
    {
        build_response(res, 403, "Forbidden (not member)", NULL);
        return 0;
    }

    if (me->role != ROLE_OWNER)
    {
        build_response(res, 403, "Forbidden (owner only)", NULL);
        return 0;
    }

    Task *t = find_task_by_id(tid);
    if (!t || t->project_id != pid)
    {
        build_response(res, 404, "Task not found", NULL);
        return 0;
    }

    User *assignee = find_user_by_id(assignee_id);
    if (!assignee)
    {
        build_response(res, 404, "User not found", NULL);
        return 0;
    }

    if (!find_membership(pid, assignee_id))
    {
        build_response(res, 403, "Assignee not member of project", NULL);
        return 0;
    }

    t->assignee_id = assignee_id;
    storage_save_tasks();

    build_response(res, 200, "Assign task success", NULL);
    log_info("ASSIGN_TASK taskId=%d(title=%s) assigneeId=%d byUserId=%d",
             t->id, t->title, assignee->id, c->user_id);
    return 0;
}

int handle_update_task_status(Client *c, const char *msg, char *res)
{
    char pid_str[32], tid_str[32], status_str[32];

    if (!find_field(msg, "ProjectId", pid_str, sizeof(pid_str)) ||
        !find_field(msg, "TaskId", tid_str, sizeof(tid_str)) ||
        !find_field(msg, "Status", status_str, sizeof(status_str)))
    {
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    int pid = atoi(pid_str);
    int tid = atoi(tid_str);
    int status_val = atoi(status_str);

    if (pid <= 0 || tid <= 0)
    {
        build_response(res, 400, "Invalid ID", NULL);
        return 0;
    }

    if (status_val < 0 || status_val > 2)
    {
        build_response(res, 422, "Status invalid", NULL);
        return 0;
    }

    Project *p = find_project_by_id(pid);
    if (!p)
    {
        build_response(res, 404, "Project not found", NULL);
        return 0;
    }

    Membership *me = find_membership(pid, c->user_id);
    if (!me)
    {
        build_response(res, 403, "Forbidden (not member)", NULL);
        return 0;
    }

    Task *t = find_task_by_id(tid);
    if (!t || t->project_id != pid)
    {
        build_response(res, 404, "Task not found", NULL);
        return 0;
    }

    TaskStatus newst = (TaskStatus)status_val;

    if (me->role == ROLE_MEMBER && t->assignee_id != c->user_id)
    {
        build_response(res, 403, "Forbidden (not assignee)", NULL);
        return 0;
    }

    if (t->status == DONE && newst == DONE)
    {
        build_response(res, 409, "Task already completed", NULL);
        return 0;
    }

    t->status = newst;
    storage_save_tasks();

    build_response(res, 200, "Update task status success", NULL);
    log_info("UPDATE_TASK_STATUS taskId=%d(title=%s) status=%d byUserId=%d",
             t->id, t->title, newst, c->user_id);
    return 0;
}

#include "project_srv.h"
#include "storage.h"
#include "protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int valid_project_name(const char *s)
{
    if (!s)
        return 0;
    int n = (int)strlen(s);
    return n >= 3 && n <= 63;
}

int handle_list_projects(Client *c, const char *msg, char *res)
{
    (void)msg;

    char extra[MSG_MAX];

    int pos = 0, count = 0;

    for (int i = 0; i < members_count; i++)
    {
        if (members[i].user_id == c->user_id)
        {
            Project *p = find_project_by_id(members[i].project_id);
            if (!p)
                continue;

            const char *role_str =
                (members[i].role == ROLE_OWNER) ? "OWNER" : "MEMBER";

            pos += snprintf(extra + pos, sizeof(extra) - pos,
                            "P%d: %d|%s|%s|%d\r\n",
                            count + 1, p->id, p->name, role_str, p->status);
            count++;
            if (pos >= (int)sizeof(extra))
                break;
        }
    }

    char final_extra[MSG_MAX];
    int pos2 = 0;

    pos2 += snprintf(final_extra + pos2, sizeof(final_extra) - pos2,
                     "Count: %d\r\n", count);
    pos2 += snprintf(final_extra + pos2, sizeof(final_extra) - pos2,
                     "%s", extra);

    build_response(res, 200, "List projects success", final_extra);
    log_info("LIST_PROJECTS userId=%d count=%d",
             c->user_id, count);
    return 0;
}

int handle_search_project(Client *c, const char *msg, char *res)
{
    (void)c;

    char kw[128];
    if (!find_field(msg, "Keyword", kw, sizeof(kw)) || strlen(kw) == 0)
    {
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    char listbuf[MSG_MAX];
    char extra[MSG_MAX];

    int pos = 0, count = 0;

    for (int i = 0; i < projects_count; i++)
    {
        if (strstr(projects[i].name, kw))
        {
            User *owner = find_user_by_id(projects[i].owner_id);
            const char *owner_name = owner ? owner->username : "unknown";

            pos += snprintf(listbuf + pos, sizeof(listbuf) - pos,
                            "P%d: %d|%s|%s|%d\r\n",
                            count + 1, projects[i].id, projects[i].name, owner_name, projects[i].status);
            count++;
            if (pos >= (int)sizeof(listbuf))
                break;
        }
    }

    int pos2 = 0;
    pos2 += snprintf(extra + pos2, sizeof(extra) - pos2,
                     "Count: %d\r\n", count);
    pos2 += snprintf(extra + pos2, sizeof(extra) - pos2,
                     "%s", listbuf);

    build_response(res, 200, "Search projects success", extra);
    log_info("SEARCH_PROJECT keyword=%s userId=%d count=%d",
             kw, c->user_id, count);
    return 0;
}

int handle_create_project(Client *c, const char *msg, char *res)
{
    char name[128];

    if (!find_field(msg, "Name", name, sizeof(name)))
    {
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }
    if (!valid_project_name(name))
    {
        build_response(res, 422, "Project name invalid", NULL);
        return 0;
    }

    if (find_project_by_name(name))
    {
        build_response(res, 409, "Project name already exists", NULL);
        return 0;
    }

    if (projects_count >= MAX_PROJECTS)
    {
        build_response(res, 507, "Storage full", NULL);
        return 0;
    }

    Project p;
    memset(&p, 0, sizeof(p));
    p.id = next_project_id();
    strncpy(p.name, name, sizeof(p.name) - 1);
    p.name[sizeof(p.name) - 1] = 0;
    p.owner_id = c->user_id;
    p.status = TODO;

    projects[projects_count++] = p;
    storage_save_projects();

    if (members_count < MAX_MEMBERS)
    {
        Membership m;
        memset(&m, 0, sizeof(m));
        m.project_id = p.id;
        m.user_id = c->user_id;
        m.role = ROLE_OWNER;
        members[members_count++] = m;
        storage_save_members();
    }

    char extra[64];
    snprintf(extra, sizeof(extra), "ProjectId: %d", p.id);
    build_response(res, 201, "Create project success", extra);
    log_info("CREATE_PROJECT projectId=%d name=%s ownerId=%d",
             p.id, p.name, p.owner_id);
    return 0;
}

int handle_add_member(Client *c, const char *msg, char *res)
{
    char pid_str[32], uname[64];

    if (!find_field(msg, "ProjectId", pid_str, sizeof(pid_str)) ||
        !find_field(msg, "Username", uname, sizeof(uname)))
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
    if (!me || me->role != ROLE_OWNER)
    {
        build_response(res, 403, "Forbidden (owner only)", NULL);
        return 0;
    }

    User *u = find_user_by_username(uname);
    if (!u)
    {
        build_response(res, 404, "User not found", NULL);
        return 0;
    }

    if (find_membership(pid, u->id))
    {
        build_response(res, 409, "Already a member", NULL);
        return 0;
    }

    if (members_count >= MAX_MEMBERS)
    {
        build_response(res, 507, "Storage full", NULL);
        return 0;
    }

    Membership m;
    memset(&m, 0, sizeof(m));
    m.project_id = pid;
    m.user_id = u->id;
    m.role = ROLE_MEMBER;
    members[members_count++] = m;
    storage_save_members();

    build_response(res, 200, "Add member success", NULL);
    log_info("ADD_MEMBER projectId=%d memberId=%d byOwnerId=%d",
             pid, u->id, c->user_id);
    return 0;
}

int handle_list_members(Client *c, const char *msg, char *res)
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

    char memberbuf[MSG_MAX];
    int mcount = list_project_member(pid, memberbuf, sizeof(memberbuf));

    char extra[MSG_MAX];
    snprintf(extra, sizeof(extra), "MemberCount: %d\r\n%s", mcount, memberbuf);

    build_response(res, 200, "List members success", extra);
    return 0;
}

int handle_update_project_status(Client *c, const char *msg, char *res)
{
    char pid_str[32], status_str[32];

    if (!find_field(msg, "ProjectId", pid_str, sizeof(pid_str)) ||
        !find_field(msg, "Status", status_str, sizeof(status_str)))
    {
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    int pid = atoi(pid_str);
    int status_val = atoi(status_str);

    if (pid <= 0)
    {
        build_response(res, 400, "Invalid project ID", NULL);
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

    if (me->role != ROLE_OWNER)
    {
        build_response(res, 403, "Forbidden (owner only)", NULL);
        return 0;
    }

    TaskStatus newst = (TaskStatus)status_val;
    p->status = newst;
    storage_save_projects();

    build_response(res, 200, "Update project status success", NULL);
    log_info("UPDATE_PROJECT_STATUS projectId=%d status=%d byUserId=%d",
             p->id, newst, c->user_id);
    return 0;
}

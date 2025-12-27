#include "storage.h"
#include <ctype.h>

User users[MAX_USERS];
int users_count = 0;
Project projects[MAX_PROJECTS];
int projects_count = 0;
Membership members[MAX_MEMBERS];
int members_count = 0;
Task tasks[MAX_TASKS];
int tasks_count = 0;
Comment comments[MAX_COMMENTS];
int comments_count = 0;

static int extract_int(const char *line, const char *key, int *out)
{
    char pat[64];
    sprintf(pat, "\"%s\":", key);
    char *p = strstr(line, pat);
    if (!p)
        return 0;
    p += strlen(pat);
    return sscanf(p, "%d", out) == 1;
}

static int extract_long(const char *line, const char *key, long *out)
{
    char pat[64];
    sprintf(pat, "\"%s\":", key);
    char *p = strstr(line, pat);
    if (!p)
        return 0;
    p += strlen(pat);
    return sscanf(p, "%ld", out) == 1;
}

static int extract_str(const char *line, const char *key, char *out, int maxlen)
{
    char pat[64];
    sprintf(pat, "\"%s\":\"", key);
    char *p = strstr(line, pat);
    if (!p)
        return 0;
    p += strlen(pat);
    char *q = strchr(p, '"');
    if (!q)
        return 0;
    int len = q - p;
    if (len >= maxlen)
        len = maxlen - 1;
    strncpy(out, p, len);
    out[len] = 0;
    return 1;
}

static int load_users()
{
    users_count = 0;
    FILE *f = fopen("data/users.jsonl", "r");
    if (!f)
        return 0;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        if (users_count >= MAX_USERS)
            break;
        User u;
        memset(&u, 0, sizeof(u));
        extract_int(line, "id", &u.id);
        extract_str(line, "username", u.username, sizeof(u.username));
        extract_str(line, "password", u.password, sizeof(u.password));
        users[users_count++] = u;
    }
    fclose(f);
    return 1;
}

static int load_projects()
{
    projects_count = 0;
    FILE *f = fopen("data/projects.jsonl", "r");
    if (!f)
        return 0;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        if (projects_count >= MAX_PROJECTS)
            break;
        Project p;
        memset(&p, 0, sizeof(p));
        if (!extract_int(line, "id", &p.id))
            continue;
        extract_str(line, "name", p.name, sizeof(p.name));
        extract_int(line, "owner_id", &p.owner_id);
        projects[projects_count++] = p;
    }
    fclose(f);
    return 1;
}

static int load_members()
{
    members_count = 0;
    FILE *f = fopen("data/members.jsonl", "r");
    if (!f)
        return 0;
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        if (members_count >= MAX_MEMBERS)
            break;
        Membership m;
        memset(&m, 0, sizeof(m));
        if (!extract_int(line, "project_id", &m.project_id))
            continue;
        extract_int(line, "user_id", &m.user_id);
        int roleInt = 1;
        extract_int(line, "role", &roleInt);
        m.role = (Role)roleInt;
        members[members_count++] = m;
    }
    fclose(f);
    return 1;
}

static int load_tasks()
{
    tasks_count = 0;
    FILE *f = fopen("data/tasks.jsonl", "r");
    if (!f)
        return 0;
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        if (tasks_count >= MAX_TASKS)
            break;
        Task t;
        memset(&t, 0, sizeof(t));
        if (!extract_int(line, "id", &t.id))
            continue;
        extract_int(line, "project_id", &t.project_id);
        extract_str(line, "title", t.title, sizeof(t.title));
        extract_str(line, "desc", t.desc, sizeof(t.desc));
        extract_int(line, "assignee_id", &t.assignee_id);
        int st = 0;
        extract_int(line, "status", &st);
        t.status = (TaskStatus)st;
        tasks[tasks_count++] = t;
    }
    fclose(f);
    return 1;
}

static int load_comments()
{
    comments_count = 0;
    FILE *f = fopen("data/comments.jsonl", "r");
    if (!f)
        return 0;
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        if (comments_count >= MAX_COMMENTS)
            break;
        Comment c;
        memset(&c, 0, sizeof(c));
        if (!extract_int(line, "id", &c.id))
            continue;
        extract_int(line, "task_id", &c.task_id);
        extract_int(line, "author_id", &c.author_id);
        extract_str(line, "content", c.content, sizeof(c.content));
        extract_long(line, "created_at", &c.created_at);
        comments[comments_count++] = c;
    }
    fclose(f);
    return 1;
}

User *find_user_by_username(const char *username)
{
    for (int i = 0; i < users_count; i++)
        if (strcmp(users[i].username, username) == 0)
            return &users[i];
    return NULL;
}

User *find_user_by_id(int id)
{
    for (int i = 0; i < users_count; i++)
        if (users[i].id == id)
            return &users[i];
    return NULL;
}

Project *find_project_by_id(int id)
{
    for (int i = 0; i < projects_count; i++)
        if (projects[i].id == id)
            return &projects[i];
    return NULL;
}

Project *find_project_by_name(const char *name)
{
    if (!name)
        return NULL;
    for (int i = 0; i < projects_count; i++)
    {
        if (strcmp(projects[i].name, name) == 0)
            return &projects[i];
    }
    return NULL;
}

Membership *find_membership(int project_id, int user_id)
{
    for (int i = 0; i < members_count; i++)
        if (members[i].project_id == project_id && members[i].user_id == user_id)
            return &members[i];
    return NULL;
}



Task *find_task_by_id(int task_id)
{
    for (int i = 0; i < tasks_count; i++)
    {
        if (tasks[i].id == task_id)
            return &tasks[i];
    }
    return NULL;
}

int list_project_member(int project_id, char *out, int maxlen)
{
    out[0] = 0;
    int pos = 0, count = 0;
    for (int i = 0; i < members_count; i++)
    {
        if (members[i].project_id == project_id)
        {
            User *u = find_user_by_id(members[i].user_id);
            if (!u) continue;
            const char *role_str = (members[i].role == ROLE_OWNER) ? "OWNER" : "MEMBER";
            pos += snprintf(out + pos, maxlen - pos, "M%d: %d|%s|%s\r\n",
                           count + 1, u->id, u->username, role_str);
            count++;
            if (pos >= maxlen) break;
        }
    }
    return count;
}

Task *find_task_by_title(int project_id, const char *title)
{
    if (!title)
        return NULL;
    for (int i = 0; i < tasks_count; i++)
    {
        if (tasks[i].project_id == project_id &&
            strcmp(tasks[i].title, title) == 0)
        {
            return &tasks[i];
        }
    }
    return NULL;
}

int count_project_by_name(const char *name)
{
    int cnt = 0;
    if (!name)
        return 0;
    for (int i = 0; i < projects_count; i++)
    {
        if (strcmp(projects[i].name, name) == 0)
            cnt++;
    }
    return cnt;
}

int count_task_by_title(int project_id, const char *title)
{
    int cnt = 0;
    if (!title)
        return 0;
    for (int i = 0; i < tasks_count; i++)
    {
        if (tasks[i].project_id == project_id &&
            strcmp(tasks[i].title, title) == 0)
        {
            cnt++;
        }
    }
    return cnt;
}

int next_user_id()
{
    return users_count == 0 ? 1 : users[users_count - 1].id + 1;
}
int next_project_id()
{
    return projects_count == 0 ? 1 : projects[projects_count - 1].id + 1;
}
int next_task_id()
{
    return tasks_count == 0 ? 1 : tasks[tasks_count - 1].id + 1;
}
int next_comment_id()
{
    return comments_count == 0 ? 1 : comments[comments_count - 1].id + 1;
}

int storage_load_all()
{
    load_users();
    load_projects();
    load_members();
    load_tasks();
    load_comments();
    return 1;
}

int storage_save_users()
{
    FILE *f = fopen("data/users.jsonl", "w");
    if (!f)
        return 0;
    for (int i = 0; i < users_count; i++)
    {
        fprintf(f, "{\"id\":%d,\"username\":\"%s\",\"password\":\"%s\"}\n",
                users[i].id, users[i].username, users[i].password);
    }
    fclose(f);
    return 1;
}

int storage_save_projects()
{
    FILE *f = fopen("data/projects.jsonl", "w");
    if (!f)
        return 0;
    for (int i = 0; i < projects_count; i++)
    {
        fprintf(f, "{\"id\":%d,\"name\":\"%s\",\"owner_id\":%d}\n",
                projects[i].id, projects[i].name, projects[i].owner_id);
    }
    fclose(f);
    return 1;
}

int storage_save_members()
{
    FILE *f = fopen("data/members.jsonl", "w");
    if (!f)
        return 0;
    for (int i = 0; i < members_count; i++)
    {
        fprintf(f, "{\"project_id\":%d,\"user_id\":%d,\"role\":%d}\n",
                members[i].project_id, members[i].user_id, members[i].role);
    }
    fclose(f);
    return 1;
}

int storage_save_tasks()
{
    FILE *f = fopen("data/tasks.jsonl", "w");
    if (!f)
        return 0;
    for (int i = 0; i < tasks_count; i++)
    {
        fprintf(f, "{\"id\":%d,\"project_id\":%d,\"title\":\"%s\",\"desc\":\"%s\","
                   "\"assignee_id\":%d,\"status\":%d}\n",
                tasks[i].id, tasks[i].project_id, tasks[i].title, tasks[i].desc,
                tasks[i].assignee_id, tasks[i].status);
    }
    fclose(f);
    return 1;
}

int storage_save_comments()
{
    FILE *f = fopen("data/comments.jsonl", "w");
    if (!f)
        return 0;
    for (int i = 0; i < comments_count; i++)
    {
        fprintf(f, "{\"id\":%d,\"task_id\":%d,\"author_id\":%d,\"content\":\"%s\","
                   "\"created_at\":%ld}\n",
                comments[i].id, comments[i].task_id, comments[i].author_id,
                comments[i].content, comments[i].created_at);
    }
    fclose(f);
    return 1;
}

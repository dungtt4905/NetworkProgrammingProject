#include "project_srv.h"
#include "storage.h"
#include "middleware.h"
#include "protocol.h"
#include "log.h"

static const char *get_field(Request *req, const char *key)
{
  for (int i = 0; i < req->count; i++)
    if (strcasecmp(req->keys[i], key) == 0)
      return req->values[i];
  return NULL;
}

static int valid_project_name(const char *s)
{
  if (!s)
    return 0;
  int n = strlen(s);
  return n >= 3 && n <= 63;
}

int handle_list_projects(Client *c, Request *req, char *res)
{
  (void)req;
  if (require_login(c, res))
    return 0;

  char extra[MSG_MAX];
  int pos = 0, count = 0;

  for (int i = 0; i < members_count; i++)
  {
    if (members[i].user_id == c->user_id)
    {
      Project *p = find_project_by_id(members[i].project_id);
      if (!p)
        continue;
      const char *role_str = (members[i].role == ROLE_OWNER) ? "OWNER" : "MEMBER";
      pos += snprintf(extra + pos, sizeof(extra) - pos,
                      "P%d: %d|%s|%s\r\n",
                      count + 1, p->id, p->name, role_str);
      count++;
    }
  }

  char header[64];
  snprintf(header, sizeof(header), "Count: %d\r\n", count);

  char final_extra[MSG_MAX];
  snprintf(final_extra, sizeof(final_extra), "%s%s", header, extra);

  build_response(res, 203, "List projects success", final_extra);
  return 0;
}

int handle_search_project(Client *c, Request *req, char *res)
{
  if (require_login(c, res))
    return 0;

  const char *kw = get_field(req, "Keyword");
  if (!kw || strlen(kw) == 0)
  {
    build_response(res, 219, "Search keyword empty", NULL);
    return 0;
  }

  char extra[MSG_MAX];
  int pos = 0, count = 0;

  for (int i = 0; i < projects_count; i++)
  {
    if (strstr(projects[i].name, kw))
    {
      User *owner = find_user_by_id(projects[i].owner_id);
      const char *owner_name = owner ? owner->username : "unknown";
      pos += snprintf(extra + pos, sizeof(extra) - pos,
                      "P%d: %d|%s|%s\r\n",
                      count + 1, projects[i].id, projects[i].name, owner_name);
      count++;
    }
  }

  char header[64];
  snprintf(header, sizeof(header), "Count: %d\r\n", count);

  char final_extra[MSG_MAX];
  snprintf(final_extra, sizeof(final_extra), "%s%s", header, extra);

  build_response(res, 204, "Search projects success", final_extra);
  return 0;
}

int handle_create_project(Client *c, Request *req, char *res)
{
  if (require_login(c, res))
    return 0;

  const char *name = get_field(req, "Name");
  if (!valid_project_name(name))
  {
    build_response(res, 212, "Project name invalid", NULL);
    return 0;
  }
  if (projects_count >= MAX_PROJECTS)
  {
    build_response(res, 510, "Project storage full", NULL);
    return 0;
  }

  Project p;
  memset(&p, 0, sizeof(p));
  p.id = next_project_id();
  strncpy(p.name, name, sizeof(p.name) - 1);
  p.owner_id = c->user_id;
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
  log_info("CREATE_PROJECT id=%d name=%s owner=%d", p.id, p.name, p.owner_id);
  return 0;
}

int handle_add_member(Client *c, Request *req, char *res)
{
  if (require_login(c, res))
    return 0;

  const char *pid_s = get_field(req, "ProjectId");
  const char *uname = get_field(req, "Username");
  if (!pid_s || !uname)
  {
    build_response(res, 512, "Missing field", NULL);
    return 0;
  }
  int pid = atoi(pid_s);

  Project *p = find_project_by_id(pid);
  if (!p)
  {
    build_response(res, 211, "Project not found", NULL);
    return 0;
  }

  if (require_role(c, pid, ROLE_OWNER, 215, res))
    return 0;

  User *u = find_user_by_username(uname);
  if (!u)
  {
    build_response(res, 217, "Target user not found", NULL);
    return 0;
  }

  if (find_membership(pid, u->id))
  {
    build_response(res, 213, "Already a member", NULL);
    return 0;
  }

  if (members_count >= MAX_MEMBERS)
  {
    build_response(res, 510, "Member storage full", NULL);
    return 0;
  }

  Membership m;
  memset(&m, 0, sizeof(m));
  m.project_id = pid;
  m.user_id = u->id;
  m.role = ROLE_MEMBER;
  members[members_count++] = m;
  storage_save_members();

  build_response(res, 202, "Add member success", NULL);
  log_info("ADD_MEMBER project=%d user=%d by_owner=%d", pid, u->id, c->user_id);
  return 0;
}
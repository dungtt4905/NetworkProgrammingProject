#pragma once
#include "common.h"

// bộ nhớ RAM toàn cục
extern User users[MAX_USERS];
extern int users_count;

extern Project projects[MAX_PROJECTS];
extern int projects_count;

extern Membership members[MAX_MEMBERS];
extern int members_count;

extern Task tasks[MAX_TASKS];
extern int tasks_count;

extern Comment comments[MAX_COMMENTS];
extern int comments_count;

// load/save tất cả
int storage_load_all();
int storage_save_users();
int storage_save_projects();
int storage_save_members();
int storage_save_tasks();
int storage_save_comments();

// tiện ích tìm kiếm
User* find_user_by_username(const char *username);
User* find_user_by_id(int id);
Project* find_project_by_name(const char *name);
int count_project_by_name(const char *name);
Project* find_project_by_id(int id);

Membership* find_membership(int project_id, int user_id);
Task* find_task_by_title(int project_id, const char *title);
int count_task_by_title(int project_id, const char *title);

// sinh id mới
int next_user_id();
int next_project_id();
int next_task_id();
int next_comment_id();

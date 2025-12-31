#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_USERS 1000
#define MAX_PROJECTS 1000
#define MAX_MEMBERS 5000
#define MAX_TASKS 5000
#define MAX_COMMENTS 10000

#define BUF_CHUNK 1024
#define MSG_MAX 8192

typedef enum
{
    ROLE_OWNER = 0,
    ROLE_MEMBER = 1,
    ROLE_OUTSIDER = 2
} Role;
typedef enum
{
    TODO = 0,
    IN_PROGRESS = 1,
    DONE = 2
} TaskStatus;

typedef struct
{
    int id;
    char username[32];
    char password[64];
} User;

typedef struct
{
    int id;
    char name[64];
    int owner_id;
    TaskStatus status;
} Project;

typedef struct
{
    int project_id;
    int user_id;
    Role role;
} Membership;

typedef struct
{
    int id;
    int project_id;
    char title[64];
    char desc[256];
    int assignee_id;
    TaskStatus status;
} Task;

typedef struct
{
    int id;
    int task_id;
    int author_id;
    char content[256];
    long created_at;
} Comment;

typedef struct
{
    int fd;
    int user_id;
    char session[64];
    char inbuf[MSG_MAX];
    int inlen;
} Client;

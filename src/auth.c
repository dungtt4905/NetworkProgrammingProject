#include "auth.h"
#include "storage.h"
#include "protocol.h"
#include "log.h"
#include <ctype.h>

static int valid_username(const char *u){
    if(!u || strlen(u)<3 || strlen(u)>31) return 0;
    for(const char *p=u; *p; p++){
        if(!(isalnum(*p) || *p=='_')) return 0;
    }
    return 1;
}
static int valid_password(const char *p){
    if(!p || strlen(p)<4 || strlen(p)>63) return 0;
    return 1;
}
static void make_session(char *out){
    sprintf(out, "%ld-%d", time(NULL), rand());
}

int handle_register(Client *c, const char *msg, char *res){
    (void)c;
    char username[64], password[64];

    if(!find_field(msg,"Username",username,sizeof(username)) ||
       !find_field(msg,"Password",password,sizeof(password))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    if(!valid_username(username)){
        build_response(res, 422, "Username invalid", NULL);
        return 0;
    }
    if(!valid_password(password)){
        build_response(res, 422, "Password invalid", NULL);
        return 0;
    }
    if(find_user_by_username(username)){
        build_response(res, 409, "User exists", NULL);
        return 0;
    }
    if(users_count>=MAX_USERS){
        build_response(res, 507, "Storage full", NULL);
        return 0;
    }

    User u; memset(&u,0,sizeof(u));
    u.id = next_user_id();
    strncpy(u.username, username, sizeof(u.username)-1);
    strncpy(u.password, password, sizeof(u.password)-1);

    users[users_count++] = u;
    storage_save_users();

    char extra[64];
    sprintf(extra,"UserId: %d", u.id);
    build_response(res, 201, "Register success", extra);
    return 0;
}

int handle_login(Client *c, const char *msg, char *res){
    char username[64], password[64];

    if(c->user_id >= 0){
        build_response(res, 409, "Already logged in", NULL);
        return 0;
    }

    if(!find_field(msg,"Username",username,sizeof(username)) ||
       !find_field(msg,"Password",password,sizeof(password))){
        build_response(res, 400, "Missing field", NULL);
        return 0;
    }

    User *u = find_user_by_username(username);
    if(!u){
        build_response(res, 401, "Login failed", NULL);
        return 0;
    }
    if(strcmp(u->password, password)!=0){
        build_response(res, 401, "Login failed", NULL);
        return 0;
    }

    c->user_id = u->id;
    make_session(c->session);

    char extra[128];
    sprintf(extra,"Session: %s\r\nUserId: %d", c->session, c->user_id);
    build_response(res, 200, "Login success", extra);
    return 0;
}

int handle_logout(Client *c, const char *msg, char *res){
    (void)msg;
    if(c->user_id < 0){
        build_response(res, 403, "Not logged in", NULL);
        return 0;
    }
    c->user_id = -1;
    c->session[0]=0;
    build_response(res, 202, "Logout success", NULL);
    return 0;
}

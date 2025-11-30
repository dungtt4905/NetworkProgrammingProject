#include "auth.h"
#include "storage.h"
#include "protocol.h"
#include "log.h"
#include <ctype.h>

static const char* get_field(Request *req, const char *key){
    for(int i=0;i<req->count;i++)
        if(strcasecmp(req->keys[i],key)==0) return req->values[i];
    return NULL;
}

static int valid_username(const char *u){
    if(!u || strlen(u)<3 || strlen(u)>31) return 0;
    for(const char *p=u; *p; p++){
        if(!(isalnum(*p) || *p=='_' )) return 0;
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

int handle_register(Client *c, Request *req, char *res){
    const char *username = get_field(req,"Username");
    const char *password = get_field(req,"Password");
    if(!username || !password){
        build_response(res, 512, "Missing field", NULL);
        return 0;
    }
    if(!valid_username(username)){
        build_response(res, 112, "Username invalid", NULL);
        return 0;
    }
    if(!valid_password(password)){
        build_response(res, 113, "Password invalid", NULL);
        return 0;
    }
    if(find_user_by_username(username)){
        build_response(res, 111, "Username already exists", NULL);
        return 0;
    }
    if(users_count>=MAX_USERS){
        build_response(res, 510, "User storage full", NULL);
        return 0;
    }

    User u; memset(&u,0,sizeof(u));
    u.id = next_user_id();
    strncpy(u.username, username, sizeof(u.username)-1);
    strncpy(u.password, password, sizeof(u.password)-1); // bài t?p cho plaintext
    users[users_count++] = u;
    storage_save_users();

    char extra[128];
    sprintf(extra, "UserId: %d", u.id);
    build_response(res, 101, "Register success", extra);
    log_info("REGISTER user=%s id=%d", u.username, u.id);
    return 0;
}

int handle_login(Client *c, Request *req, char *res){
    const char *username = get_field(req,"Username");
    const char *password = get_field(req,"Password");
    if(!username || !password){
        build_response(res, 512, "Missing field", NULL);
        return 0;
    }

    User *u = find_user_by_username(username);
    if(!u){
        build_response(res, 114, "User not found", NULL);
        return 0;
    }
    if(strcmp(u->password, password)!=0){
        build_response(res, 115, "Wrong password", NULL);
        return 0;
    }

    c->user_id = u->id;
    make_session(c->session);

    char extra[256];
    sprintf(extra, "Session: %s\r\nUserId: %d", c->session, c->user_id);
    build_response(res, 102, "Login success", extra);
    log_info("LOGIN user=%s id=%d session=%s fd=%d",
        u->username, u->id, c->session, c->fd);
    return 0;
}

int handle_logout(Client *c, Request *req, char *res){
    (void)req;
    if(c->user_id<0){
        build_response(res, 116, "Not logged in", NULL);
        return 0;
    }
    log_info("LOGOUT user_id=%d fd=%d", c->user_id, c->fd);
    c->user_id=-1;
    c->session[0]=0;
    build_response(res, 103, "Logout success", NULL);
    return 0;
}


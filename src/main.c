#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "protocol.h"
#include "storage.h"
#include "auth.h"
#include "middleware.h"

#include "project_srv.h"
#include "task_srv.h"

#define PORT 9090

static Client clients[FD_SETSIZE];

static void init_clients()
{
    for (int i = 0; i < FD_SETSIZE; i++)
    {
        clients[i].fd = -1;
        clients[i].user_id = -1;
        clients[i].inlen = 0;
        clients[i].session[0] = 0;
    }
}

static void add_client(int fd)
{
    for (int i = 0; i < FD_SETSIZE; i++)
    {
        if (clients[i].fd == -1)
        {
            clients[i].fd = fd;
            clients[i].user_id = -1;
            clients[i].inlen = 0;
            clients[i].session[0] = 0;
            return;
        }
    }
    close(fd);
}

static void remove_client(int i)
{
    if (clients[i].fd != -1)
        close(clients[i].fd);
    clients[i].fd = -1;
    clients[i].user_id = -1;
    clients[i].inlen = 0;
    clients[i].session[0] = 0;
}

static void dispatch(Client *c, const char *msg)
{
    char cmd[32];
    char res[MSG_MAX];

    if (!parse_command(msg, cmd))
    {
        build_response(res, 400, "Bad request format", NULL);
        send(c->fd, res, strlen(res), 0);
        return;
    }

    int is_auth_cmd =
        strcmp(cmd, "REGISTER") == 0 ||
        strcmp(cmd, "LOGIN") == 0;

    if (!is_auth_cmd && c->user_id < 0)
    {
        build_response(res, 403, "Not logged in", NULL);
        send(c->fd, res, strlen(res), 0);
        return;
    }

    if (strcmp(cmd, "REGISTER") == 0)
        handle_register(c, msg, res);
    else if (strcmp(cmd, "LOGIN") == 0)
        handle_login(c, msg, res);
    else if (strcmp(cmd, "LOGOUT") == 0)
        handle_logout(c, msg, res);

    else if (strcmp(cmd, "LIST_PROJECTS") == 0)
        handle_list_projects(c, msg, res);
    else if (strcmp(cmd, "SEARCH_PROJECT") == 0)
        handle_search_project(c, msg, res);
    else if (strcmp(cmd, "CREATE_PROJECT") == 0)
        handle_create_project(c, msg, res);
    else if (strcmp(cmd, "ADD_MEMBER") == 0)
        handle_add_member(c, msg, res);
    else if (strcmp(cmd, "LIST_MEMBERS") == 0)
        handle_list_members(c, msg, res);
    else if (strcmp(cmd, "LIST_TASKS") == 0)
        handle_list_tasks(c, msg, res);
    else if (strcmp(cmd, "CREATE_TASK") == 0)
        handle_create_task(c, msg, res);
    else if (strcmp(cmd, "ASSIGN_TASK") == 0)
        handle_assign_task(c, msg, res);
    else if (strcmp(cmd, "UPDATE_TASK_STATUS") == 0)
        handle_update_task_status(c, msg, res);

    else if (strcmp(cmd, "COMMENT_TASK") == 0)
        handle_comment_task(c, msg, res);

    else
        build_response(res, 400, "Unknown command", NULL);

    send(c->fd, res, strlen(res), 0);
}

int main()
{
    storage_load_all();
    init_clients();

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 10) < 0)
    {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    log_info("Server started on port %d", PORT);
    printf("Server running on port %d...\n", PORT);

    while (1)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        int maxfd = listen_fd;

        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (clients[i].fd != -1)
            {
                FD_SET(clients[i].fd, &rfds);
                if (clients[i].fd > maxfd)
                    maxfd = clients[i].fd;
            }
        }

        int nready = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (nready < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        if (FD_ISSET(listen_fd, &rfds))
        {
            int cfd = accept(listen_fd, NULL, NULL);
            if (cfd >= 0)
            {
                add_client(cfd);
                log_info("Client connected: fd=%d", cfd);
            }
        }

        for (int i = 0; i < FD_SETSIZE; i++)
        {
            Client *c = &clients[i];
            if (c->fd == -1)
                continue;

            if (FD_ISSET(c->fd, &rfds))
            {
                char buf[BUF_CHUNK];
                int n = recv(c->fd, buf, sizeof(buf), 0);

                if (n <= 0)
                {
                    log_info("Client disconnected: fd=%d | user_id=%d", c->fd, c->user_id);
                    remove_client(i);
                    continue;
                }

                if (c->inlen + n >= MSG_MAX - 1)
                {
                    char rtmp[256];
                    build_response(rtmp, 400, "Message too large", NULL);
                    send(c->fd, rtmp, strlen(rtmp), 0);
                    log_info("REQ too large: fd=%d", c->fd);
                    c->inlen = 0;
                    continue;
                }

                memcpy(c->inbuf + c->inlen, buf, n);
                c->inlen += n;

                char msg[MSG_MAX];
                while (try_extract_message(c, msg))
                {
                    log_info("REQ fd=%d: %.*s", c->fd, 120, msg);
                    dispatch(c, msg);
                }
            }
        }
    }

    close(listen_fd);
    return 0;
}

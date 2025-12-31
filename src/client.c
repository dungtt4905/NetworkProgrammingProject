#include "client_common.h"

static int logged_in = 0;
static char current_user[32] = "";
static char session[64] = "";

static int connect_server(const char *ip, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        close(sock);
        return -1;
    }
    return sock;
}

static void send_req(int sock, const char *req)
{
    send(sock, req, strlen(req), 0);
}

static void show_response(int sock)
{
    char resp[CBUF];
    if (!recv_response(sock, resp, sizeof(resp)))
    {
        printf("Server closed connection.\n");
        exit(0);
    }
    printf("\n----- RESPONSE -----\n%s--------------------\n", resp);

    if (strstr(resp, "Login success") != NULL)
    {
        logged_in = 1;
        char *p = strstr(resp, "Session:");
        if (p)
        {
            p += strlen("Session:");
            while (*p == ' ')
                p++;
            sscanf(p, "%63s", session);
        }
    }

    if (strstr(resp, "Logout success") != NULL)
    {
        logged_in = 0;
        session[0] = 0;
        current_user[0] = 0;
    }
}

static int check_error_response(const char *resp)
{
    const char *p = strstr(resp, "RES ");
    if (p == NULL)
        return 0;

    int code = 0;
    if (sscanf(p, "RES %d", &code) == 1)
    {
        return (code != 200 && code != 201);
    }
    return 0;
}

static void menu()
{
    printf("\n========= PROJECT MANAGER CLIENT =========\n");
    printf("0. Exit\n");
    printf("1. Register\n");
    printf("2. Login\n");
    printf("3. Logout\n");
    printf("4. List my projects\n");
    printf("5. Search project by name\n");
    printf("6. Create project (owner)\n");
    printf("7. Add member to project (owner)\n");
    printf("8. Update project status (owner)\n");
    printf("9. List tasks in project\n");
    printf("10. Create task (owner)\n");
    printf("11. Assign task (owner)\n");
    printf("12. Update task status (owner)\n");
    printf("13. Comment task\n");

    if (logged_in)
        printf("Logged in as: %s | session=%s\n", current_user, session);
    else
        printf("Not logged in.\n");
    printf("============================================\n");
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 9090\n", argv[0]);
        return 1;
    }
 
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return 1;
    }
 
    int sock = connect_server(ip, port);
    if (sock < 0) return 1;
 
    printf("Connected to server %s:%d\n", ip, port);

    while (1)
    {
        menu();
        char choice_s[16];
        input_line("Choose: ", choice_s, sizeof(choice_s));
        int choice = atoi(choice_s);

        char req[CBUF];
        char a[256], b[256], c[256];
        char cmt[256];

        switch (choice)
        {
        case 0:
            close(sock);
            return 0;

        case 1:
            input_line("Username: ", a, sizeof(a));
            input_line("Password: ", b, sizeof(b));
            snprintf(req, sizeof(req),
                     "CMD REGISTER\r\nUsername: %s\r\nPassword: %s\r\n\r\n",
                     a, b);
            send_req(sock, req);
            show_response(sock);
            break;

        case 2:
            input_line("Username: ", a, sizeof(a));
            input_line("Password: ", b, sizeof(b));
            strncpy(current_user, a, sizeof(current_user) - 1);
            current_user[sizeof(current_user) - 1] = 0;
            snprintf(req, sizeof(req),
                     "CMD LOGIN\r\nUsername: %s\r\nPassword: %s\r\n\r\n",
                     a, b);
            send_req(sock, req);
            show_response(sock);
            break;

        case 3:
            snprintf(req, sizeof(req), "CMD LOGOUT\r\n\r\n");
            send_req(sock, req);
            show_response(sock);
            break;

        case 4:
            snprintf(req, sizeof(req), "CMD LIST_PROJECTS\r\n\r\n");
            send_req(sock, req);
            show_response(sock);
            break;

        case 5:
            input_line("Keyword: ", a, sizeof(a));
            snprintf(req, sizeof(req),
                     "CMD SEARCH_PROJECT\r\nKeyword: %s\r\n\r\n", a);
            send_req(sock, req);
            show_response(sock);
            break;

        case 6:
            input_line("Project name: ", a, sizeof(a));
            snprintf(req, sizeof(req),
                     "CMD CREATE_PROJECT\r\nName: %s\r\n\r\n", a);
            send_req(sock, req);
            show_response(sock);
            break;

        case 7:
        {
            snprintf(req, sizeof(req), "CMD LIST_PROJECTS\r\n\r\n");
            send_req(sock, req);
            char proj_resp[CBUF];
            if (!recv_response(sock, proj_resp, sizeof(proj_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- YOUR PROJECTS -----\n%s", proj_resp);

            input_line("Enter project ID: ", a, sizeof(a));
            int proj_id = atoi(a);
            if (proj_id <= 0)
            {
                printf("Invalid project ID.\n");
                break;
            }

            snprintf(req, sizeof(req),
                     "CMD LIST_MEMBERS\r\nProjectId: %d\r\n\r\n", proj_id);
            send_req(sock, req);
            char member_resp[CBUF];
            if (!recv_response(sock, member_resp, sizeof(member_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- CURRENT MEMBERS -----\n%s", member_resp);
            if (check_error_response(member_resp))
            {
                printf("Cannot proceed due to error above.\n");
                break;
            }

            input_line("Username to add: ", b, sizeof(b));

            snprintf(req, sizeof(req),
                     "CMD ADD_MEMBER\r\n"
                     "ProjectId: %d\r\n"
                     "Username: %s\r\n"
                     "\r\n",
                     proj_id, b);
            send_req(sock, req);
            show_response(sock);
            break;
        }

        case 8:
        {
            snprintf(req, sizeof(req), "CMD LIST_PROJECTS\r\n\r\n");
            send_req(sock, req);
            char proj_resp[CBUF];
            if (!recv_response(sock, proj_resp, sizeof(proj_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- YOUR PROJECTS -----\n%s", proj_resp);

            input_line("Enter project ID: ", a, sizeof(a));
            int proj_id = atoi(a);
            if (proj_id <= 0)
            {
                printf("Invalid project ID.\n");
                break;
            }

            printf("\n----- STATUS OPTIONS -----\n");
            printf("0: TODO\n");
            printf("1: IN PROGRESS\n");
            printf("2: DONE\n");
            input_line("Enter status number: ", b, sizeof(b));
            int status = atoi(b);
            if (status < 0 || status > 2)
            {
                printf("Invalid status.\n");
                break;
            }

            snprintf(req, sizeof(req),
                     "CMD UPDATE_PROJECT_STATUS\r\n"
                     "ProjectId: %d\r\n"
                     "Status: %d\r\n"
                     "\r\n",
                     proj_id, status);
            send_req(sock, req);
            show_response(sock);
            break;
        }

        case 9:
        {
            snprintf(req, sizeof(req), "CMD LIST_PROJECTS\r\n\r\n");
            send_req(sock, req);
            char proj_resp[CBUF];
            if (!recv_response(sock, proj_resp, sizeof(proj_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- YOUR PROJECTS -----\n%s", proj_resp);

            input_line("Enter project ID: ", a, sizeof(a));
            int proj_id = atoi(a);
            if (proj_id <= 0)
            {
                printf("Invalid project ID.\n");
                break;
            }

            snprintf(req, sizeof(req),
                     "CMD LIST_TASKS\r\nProjectId: %d\r\n\r\n", proj_id);
            send_req(sock, req);
            show_response(sock);
            break;
        }

        case 10:
        {
            snprintf(req, sizeof(req), "CMD LIST_PROJECTS\r\n\r\n");
            send_req(sock, req);
            char proj_resp[CBUF];
            if (!recv_response(sock, proj_resp, sizeof(proj_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- YOUR PROJECTS -----\n%s", proj_resp);

            input_line("Enter project ID: ", a, sizeof(a));
            int proj_id = atoi(a);
            if (proj_id <= 0)
            {
                printf("Invalid project ID.\n");
                break;
            }

            input_line("Title: ", b, sizeof(b));
            input_line("Desc: ", c, sizeof(c));

            snprintf(req, sizeof(req),
                     "CMD CREATE_TASK\r\n"
                     "ProjectId: %d\r\n"
                     "Title: %s\r\n"
                     "Desc: %s\r\n"
                     "\r\n",
                     proj_id, b, c);
            send_req(sock, req);
            show_response(sock);
            break;
        }

        case 11:
        {
            snprintf(req, sizeof(req), "CMD LIST_PROJECTS\r\n\r\n");
            send_req(sock, req);
            char proj_resp[CBUF];
            if (!recv_response(sock, proj_resp, sizeof(proj_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- YOUR PROJECTS -----\n%s", proj_resp);

            input_line("Enter project ID: ", a, sizeof(a));
            int proj_id = atoi(a);
            if (proj_id <= 0)
            {
                printf("Invalid project ID.\n");
                break;
            }

            snprintf(req, sizeof(req),
                     "CMD LIST_TASKS\r\nProjectId: %d\r\n\r\n", proj_id);
            send_req(sock, req);
            char task_resp[CBUF];
            if (!recv_response(sock, task_resp, sizeof(task_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- TASKS -----\n%s", task_resp);
            if (check_error_response(task_resp))
            {
                printf("Cannot proceed due to error above.\n");
                break;
            }

            snprintf(req, sizeof(req),
                     "CMD LIST_MEMBERS\r\nProjectId: %d\r\n\r\n", proj_id);
            send_req(sock, req);
            char member_resp[CBUF];
            if (!recv_response(sock, member_resp, sizeof(member_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- MEMBERS -----\n%s", member_resp);
            if (check_error_response(member_resp))
            {
                printf("Cannot proceed due to error above.\n");
                break;
            }

            input_line("Enter task ID: ", b, sizeof(b));
            int task_id = atoi(b);
            if (task_id <= 0)
            {
                printf("Invalid task ID.\n");
                break;
            }

            input_line("Enter assignee ID: ", c, sizeof(c));
            int assignee_id = atoi(c);
            if (assignee_id <= 0)
            {
                printf("Invalid assignee ID.\n");
                break;
            }

            snprintf(req, sizeof(req),
                     "CMD ASSIGN_TASK\r\n"
                     "ProjectId: %d\r\n"
                     "TaskId: %d\r\n"
                     "AssigneeId: %d\r\n"
                     "\r\n",
                     proj_id, task_id, assignee_id);
            send_req(sock, req);
            show_response(sock);
            break;
        }

        case 12:
        {
            snprintf(req, sizeof(req), "CMD LIST_PROJECTS\r\n\r\n");
            send_req(sock, req);
            char proj_resp[CBUF];
            if (!recv_response(sock, proj_resp, sizeof(proj_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- YOUR PROJECTS -----\n%s", proj_resp);

            input_line("Enter project ID: ", a, sizeof(a));
            int proj_id = atoi(a);
            if (proj_id <= 0)
            {
                printf("Invalid project ID.\n");
                break;
            }

            snprintf(req, sizeof(req),
                     "CMD LIST_TASKS\r\nProjectId: %d\r\n\r\n", proj_id);
            send_req(sock, req);
            char task_resp[CBUF];
            if (!recv_response(sock, task_resp, sizeof(task_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- TASKS -----\n%s", task_resp);
            if (check_error_response(task_resp))
            {
                printf("Cannot proceed due to error above.\n");
                break;
            }

            input_line("Enter task ID: ", b, sizeof(b));
            int task_id = atoi(b);
            if (task_id <= 0)
            {
                printf("Invalid task ID.\n");
                break;
            }

            printf("\n----- STATUS OPTIONS -----\n");
            printf("0: TO DO\n");
            printf("1: IN PROGRESS\n");
            printf("2: DONE\n");
            input_line("Enter status number: ", c, sizeof(c));
            int status = atoi(c);
            if (status < 0 || status > 2)
            {
                printf("Invalid status.\n");
                break;
            }

            snprintf(req, sizeof(req),
                     "CMD UPDATE_TASK_STATUS\r\n"
                     "ProjectId: %d\r\n"
                     "TaskId: %d\r\n"
                     "Status: %d\r\n"
                     "\r\n",
                     proj_id, task_id, status);
            send_req(sock, req);
            show_response(sock);
            break;
        }

        case 13:
        {
            snprintf(req, sizeof(req), "CMD LIST_PROJECTS\r\n\r\n");
            send_req(sock, req);
            char proj_resp[CBUF];
            if (!recv_response(sock, proj_resp, sizeof(proj_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- YOUR PROJECTS -----\n%s", proj_resp);

            input_line("Enter project ID: ", a, sizeof(a));
            int proj_id = atoi(a);
            if (proj_id <= 0)
            {
                printf("Invalid project ID.\n");
                break;
            }

            snprintf(req, sizeof(req),
                     "CMD LIST_TASKS\r\nProjectId: %d\r\n\r\n", proj_id);
            send_req(sock, req);
            char task_resp[CBUF];
            if (!recv_response(sock, task_resp, sizeof(task_resp)))
            {
                printf("Server closed connection.\n");
                exit(0);
            }
            printf("\n----- TASKS -----\n%s", task_resp);
            if (check_error_response(task_resp))
            {
                printf("Cannot proceed due to error above.\n");
                break;
            }

            input_line("Enter task ID: ", b, sizeof(b));
            int task_id = atoi(b);
            if (task_id <= 0)
            {
                printf("Invalid task ID.\n");
                break;
            }

            input_line("Comment: ", cmt, sizeof(cmt));

            snprintf(req, sizeof(req),
                     "CMD COMMENT_TASK\r\n"
                     "ProjectId: %d\r\n"
                     "TaskId: %d\r\n"
                     "Comment: %s\r\n"
                     "\r\n",
                     proj_id, task_id, cmt);
            send_req(sock, req);
            show_response(sock);
            break;
        }

        default:
            printf("Invalid choice.\n");
        }
    }
}

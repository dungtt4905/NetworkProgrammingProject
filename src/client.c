#include "client_common.h"

// trạng thái local (chỉ để UI đẹp hơn)
static int logged_in = 0;
static char current_user[32] = "";
static char session[64] = "";

static int connect_server(const char *ip, int port){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){ perror("socket"); return -1; }

    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if(connect(sock,(struct sockaddr*)&addr,sizeof(addr))<0){
        perror("connect"); close(sock); return -1;
    }
    return sock;
}

static void send_req(int sock, const char *req){
    send(sock, req, strlen(req), 0);
}

static void show_response(int sock){
    char resp[CBUF];
    if(!recv_response(sock, resp, sizeof(resp))){
        printf("Server closed connection.\n");
        exit(0);
    }
    printf("\n----- RESPONSE -----\n%s--------------------\n", resp);

    // bắt session trong login để hiển thị
    if(strstr(resp, "RES 102")!=NULL){
        logged_in = 1;
        char *p = strstr(resp, "Session:");
        if(p){
            p += strlen("Session:");
            while(*p==' ') p++;
            sscanf(p, "%63s", session);
        }
    }
    if(strstr(resp, "RES 103")!=NULL){
        logged_in = 0;
        session[0]=0;
        current_user[0]=0;
    }
}

static void menu(){
    printf("\n========= PROJECT MANAGER CLIENT =========\n");
    printf("0. Exit\n");
    printf("1. Register\n");
    printf("2. Login\n");
    printf("3. Logout\n");
    printf("4. List my projects\n");
    printf("5. Search project by name\n");
    printf("6. Create project (owner)\n");
    printf("7. Add member to project (owner)\n");
    printf("8. List tasks in project\n");
    printf("9. Create task (owner)\n");
    printf("10. Assign task (owner)\n");
    printf("11. Update task status\n");
    printf("12. Comment task\n");

    if(logged_in) printf("Logged in as: %s | session=%s\n", current_user, session);
    else printf("Not logged in.\n");
    printf("==========================================\n");
}

int main(int argc, char **argv){
    const char *ip = "127.0.0.1";
    int port = 9090;
    if(argc >= 2) ip = argv[1];
    if(argc >= 3) port = atoi(argv[2]);

    int sock = connect_server(ip, port);
    if(sock < 0) return 1;

    printf("Connected to server %s:%d\n", ip, port);

    while(1){
        menu();
        char choice_s[16];
        input_line("Choose: ", choice_s, sizeof(choice_s));
        int choice = atoi(choice_s);

        char req[CBUF];
        char a[256], b[256], c[256];
        char cmt[256]; // <-- FIX: khai báo biến comment

        switch(choice){
            case 0:
                close(sock);
                return 0;

            case 1: // REGISTER
                input_line("Username: ", a, sizeof(a));
                input_line("Password: ", b, sizeof(b));
                snprintf(req, sizeof(req),
                        "CMD REGISTER\r\nUsername: %s\r\nPassword: %s\r\n\r\n",
                        a, b);
                send_req(sock, req);
                show_response(sock);
                break;

            case 2: // LOGIN
                input_line("Username: ", a, sizeof(a));
                input_line("Password: ", b, sizeof(b));
                strncpy(current_user, a, sizeof(current_user)-1);
                current_user[sizeof(current_user)-1] = 0;
                snprintf(req, sizeof(req),
                        "CMD LOGIN\r\nUsername: %s\r\nPassword: %s\r\n\r\n",
                        a, b);
                send_req(sock, req);
                show_response(sock);
                break;

            case 3: // LOGOUT
                snprintf(req, sizeof(req), "CMD LOGOUT\r\n\r\n");
                send_req(sock, req);
                show_response(sock);
                break;

            case 4: // LIST_PROJECTS
                snprintf(req, sizeof(req), "CMD LIST_PROJECTS\r\n\r\n");
                send_req(sock, req);
                show_response(sock);
                break;

            case 5: // SEARCH_PROJECT
                input_line("Keyword: ", a, sizeof(a));
                snprintf(req, sizeof(req),
                        "CMD SEARCH_PROJECT\r\nKeyword: %s\r\n\r\n", a);
                send_req(sock, req);
                show_response(sock);
                break;

            case 6: // CREATE_PROJECT
                input_line("Project name: ", a, sizeof(a));
                snprintf(req, sizeof(req),
                        "CMD CREATE_PROJECT\r\nName: %s\r\n\r\n", a);
                send_req(sock, req);
                show_response(sock);
                break;

            case 7: // ADD_MEMBER (theo ProjectName)
                input_line("Project name: ", a, sizeof(a));
                input_line("Username to add: ", b, sizeof(b));
                snprintf(req, sizeof(req),
                        "CMD ADD_MEMBER\r\nProjectName: %s\r\nUsername: %s\r\n\r\n",
                        a, b);
                send_req(sock, req);
                show_response(sock);
                break;

            case 8: // LIST_TASKS (theo ProjectName)
                input_line("Project name: ", a, sizeof(a));
                snprintf(req, sizeof(req),
                        "CMD LIST_TASKS\r\nProjectName: %s\r\n\r\n", a);
                send_req(sock, req);
                show_response(sock);
                break;

            case 9: { // CREATE_TASK (theo ProjectName, KHÔNG assign)
                input_line("Project name: ", a, sizeof(a));
                input_line("Title: ", b, sizeof(b));
                input_line("Desc: ", c, sizeof(c));

                snprintf(req, sizeof(req),
                        "CMD CREATE_TASK\r\n"
                        "ProjectName: %s\r\n"
                        "Title: %s\r\n"
                        "Desc: %s\r\n"
                        "\r\n",
                        a, b, c);
                send_req(sock, req);
                show_response(sock);
                break;
            }

            case 10: { // ASSIGN_TASK (theo ProjectName + TaskTitle)
                input_line("Project name: ", a, sizeof(a));
                input_line("Task title: ", b, sizeof(b));
                input_line("Assignee username: ", c, sizeof(c));

                snprintf(req, sizeof(req),
                        "CMD ASSIGN_TASK\r\n"
                        "ProjectName: %s\r\n"
                        "TaskTitle: %s\r\n"
                        "Assignee: %s\r\n"
                        "\r\n",
                        a, b, c);
                send_req(sock, req);
                show_response(sock);
                break;
            }

            case 11: { // UPDATE_TASK_STATUS (ProjectName + TaskTitle)
                input_line("Project name: ", a, sizeof(a));
                input_line("Task title: ", b, sizeof(b));
                input_line("Status (TODO/IN_PROGRESS/DONE): ", c, sizeof(c));

                snprintf(req, sizeof(req),
                        "CMD UPDATE_TASK_STATUS\r\n"
                        "ProjectName: %s\r\n"
                        "TaskTitle: %s\r\n"
                        "Status: %s\r\n"
                        "\r\n",
                        a, b, c);
                send_req(sock, req);
                show_response(sock);
                break;
            }

            case 12: // COMMENT_TASK (ProjectName + TaskTitle)
                input_line("Project name: ", a, sizeof(a));
                input_line("Task title: ", b, sizeof(b));
                input_line("Comment: ", cmt, sizeof(cmt));

                snprintf(req, sizeof(req),
                        "CMD COMMENT_TASK\r\n"
                        "ProjectName: %s\r\n"
                        "TaskTitle: %s\r\n"
                        "Content: %s\r\n"
                        "\r\n",
                        a, b, cmt);
                send_req(sock, req);
                show_response(sock);
                break;

            default:
                printf("Invalid choice.\n");
        }
    }
}

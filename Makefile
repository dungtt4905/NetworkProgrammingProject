CC      = gcc
CFLAGS  = -Iinclude -Wall -Wextra -O2

SERVER_SRC = src/main.c src/protocol.c src/storage.c src/auth.c src/middleware.c \
             src/project_srv.c src/task_srv.c src/comment_srv.c src/log.c

CLIENT_SRC = src/client.c

all: server client

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o server

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o client

run_server: server
	./server

run_client: client
	./client

clean:
	rm -f server client

.PHONY: all server client run_server run_client clean
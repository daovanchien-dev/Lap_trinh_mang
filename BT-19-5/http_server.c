#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void *handle_client(void *arg) {
    int client = *(int *)arg;
    free(arg);

    char buf[BUFFER_SIZE];

    printf("New client connected: %d\n", client);

    while (1) {
        memset(buf, 0, sizeof(buf));

        int ret = recv(client, buf, sizeof(buf) - 1, 0);

        if (ret <= 0) {
            printf("Client %d disconnected\n", client);
            break;
        }

        buf[ret] = '\0';

        printf("Client %d sent:\n%s\n", client, buf);

        char *msg =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<html><body><h1>Xin chao cac ban</h1></body></html>\n";

        send(client, msg, strlen(msg), 0);
    }

    close(client);
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket error");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind error");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen error");
        close(server_fd);
        return 1;
    }

    printf("Server is running on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int *client = malloc(sizeof(int));

        *client = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (*client < 0) {
            perror("accept error");
            free(client);
            continue;
        }

        pthread_t tid;

        if (pthread_create(&tid, NULL, handle_client, client) != 0) {
            perror("pthread_create error");
            close(*client);
            free(client);
            continue;
        }

        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
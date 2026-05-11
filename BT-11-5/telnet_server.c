#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 2323
#define BUFFER_SIZE 1024
#define BACKLOG 5

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_info_t;

int check_login(char *user, char *pass) {
    FILE *fp = fopen("users.txt", "r");
    if (!fp) return 0;
    char line[256], u[128], p[128];
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%s %s", u, p);
        if (strcmp(u, user) == 0 && strcmp(p, pass) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

void *handle_client(void *arg) {
    client_info_t *info = (client_info_t *)arg;
    int client_fd = info->client_fd;
    char buffer[BUFFER_SIZE];
    char user[128], pass[128];
    int authenticated = 0;

    send(client_fd, "Login: ", 7, 0);
    recv(client_fd, buffer, BUFFER_SIZE, 0);
    sscanf(buffer, "%s", user);

    send(client_fd, "Password: ", 9, 0);
    recv(client_fd, buffer, BUFFER_SIZE, 0);
    sscanf(buffer, "%s", pass);

    if (check_login(user, pass)) {
        send(client_fd, "OK\n", 3, 0);
        authenticated = 1;
    } else {
        send(client_fd, "Login failed\n", 13, 0);
        close(client_fd);
        free(info);
        pthread_exit(NULL);
    }

    while (authenticated) {
        memset(buffer, 0, BUFFER_SIZE);
        send(client_fd, "cmd> ", 5, 0);
        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) break;

        buffer[strcspn(buffer, "\n")] = 0;
        if (strcmp(buffer, "exit") == 0) break;

        char command[BUFFER_SIZE + 20];
        snprintf(command, sizeof(command), "%s > out.txt", buffer);
        system(command);

        FILE *out = fopen("out.txt", "r");
        if (out) {
            char line[BUFFER_SIZE];
            while (fgets(line, sizeof(line), out)) {
                send(client_fd, line, strlen(line), 0);
            }
            fclose(out);
        } else {
            send(client_fd, "Error executing command\n", 24, 0);
        }
    }

    close(client_fd);
    free(info);
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    fd_set read_fds;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Telnet server running on port %d\n", PORT);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        select(server_fd + 1, &read_fds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &read_fds)) {
            client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
            if (client_fd < 0) continue;

            printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            client_info_t *info = malloc(sizeof(client_info_t));
            info->client_fd = client_fd;
            info->client_addr = client_addr;

            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, info);
            pthread_detach(thread);
        }
    }

    close(server_fd);
    return 0;
}
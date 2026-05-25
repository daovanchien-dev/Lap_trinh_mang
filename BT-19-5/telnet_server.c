#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 5000
#define BUFFER_SIZE 1024
#define USER_FILE "users.txt"
#define OUT_FILE "out.txt"

void send_msg(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}

int recv_line(int sock, char *buffer, int size) {
    int i = 0;
    char c;

    while (i < size - 1) {
        int n = recv(sock, &c, 1, 0);

        if (n <= 0) {
            return n;
        }

        if (c == '\n') {
            break;
        }

        if (c != '\r') {
            buffer[i++] = c;
        }
    }

    buffer[i] = '\0';
    return i;
}

int check_login(const char *username, const char *password) {
    FILE *fp = fopen(USER_FILE, "r");

    if (fp == NULL) {
        return 0;
    }

    char file_user[100];
    char file_pass[100];

    while (fscanf(fp, "%s %s", file_user, file_pass) == 2) {
        if (strcmp(username, file_user) == 0 &&
            strcmp(password, file_pass) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

void send_file_content(int sock, const char *filename) {
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        send_msg(sock, "Cannot read command output\n");
        return;
    }

    char buffer[BUFFER_SIZE];

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        send_msg(sock, buffer);
    }

    fclose(fp);
}

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char username[100];
    char password[100];
    char command[BUFFER_SIZE];
    char system_command[BUFFER_SIZE + 50];

    send_msg(client_sock, "Username: ");
    if (recv_line(client_sock, username, sizeof(username)) <= 0) {
        close(client_sock);
        return NULL;
    }

    send_msg(client_sock, "Password: ");
    if (recv_line(client_sock, password, sizeof(password)) <= 0) {
        close(client_sock);
        return NULL;
    }

    if (!check_login(username, password)) {
        send_msg(client_sock, "Login failed\n");
        close(client_sock);
        return NULL;
    }

    send_msg(client_sock, "Login success\n");
    send_msg(client_sock, "Type command, or type exit to quit\n");

    while (1) {
        send_msg(client_sock, "telnet_server> ");

        int n = recv_line(client_sock, command, sizeof(command));

        if (n <= 0) {
            break;
        }

        if (strcmp(command, "exit") == 0) {
            send_msg(client_sock, "Goodbye\n");
            break;
        }

        if (strlen(command) == 0) {
            continue;
        }

        snprintf(system_command, sizeof(system_command), "%s > %s", command, OUT_FILE);

        system(system_command);

        send_file_content(client_sock, OUT_FILE);
    }

    close(client_sock);
    return NULL;
}

int main() {
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (server_sock < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_sock, 10) < 0) {
        perror("listen");
        return 1;
    }

    printf("Telnet server running on port %d...\n", PORT);

    while (1) {
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>

#define PORT 5000
#define BUFFER_SIZE 1024

void remove_newline(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
}

int get_time_format(const char *format, char *strftime_format) {
    if (strcmp(format, "dd/mm/yyyy") == 0) {
        strcpy(strftime_format, "%d/%m/%Y");
        return 1;
    }

    if (strcmp(format, "dd/mm/yy") == 0) {
        strcpy(strftime_format, "%d/%m/%y");
        return 1;
    }

    if (strcmp(format, "mm/dd/yyyy") == 0) {
        strcpy(strftime_format, "%m/%d/%Y");
        return 1;
    }

    if (strcmp(format, "mm/dd/yy") == 0) {
        strcpy(strftime_format, "%m/%d/%y");
        return 1;
    }

    return 0;
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];

    send(client_fd,
         "Time Server Ready\n"
         "Command: GET_TIME [format]\n"
         "Formats: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, mm/dd/yy\n",
         103,
         0);

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
            break;
        }

        remove_newline(buffer);

        char command[50];
        char format[50];

        int count = sscanf(buffer, "%s %s", command, format);

        if (count != 2) {
            send(client_fd, "ERROR Invalid command\n", 22, 0);
            continue;
        }

        if (strcmp(command, "GET_TIME") != 0) {
            send(client_fd, "ERROR Invalid command\n", 22, 0);
            continue;
        }

        char strftime_format[50];

        if (!get_time_format(format, strftime_format)) {
            send(client_fd, "ERROR Invalid format\n", 21, 0);
            continue;
        }

        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        char result[100];
        strftime(result, sizeof(result), strftime_format, t);

        char response[150];
        snprintf(response, sizeof(response), "OK %s\n", result);

        send(client_fd, response, strlen(response), 0);
    }

    close(client_fd);
    pthread_exit(NULL);
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Time server is running on port %d...\n", PORT);

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("New client connected\n");

        int *pclient = malloc(sizeof(int));
        *pclient = client_fd;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
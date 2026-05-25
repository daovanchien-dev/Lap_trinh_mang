#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define PORT 5000
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    char id[100];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void get_time_string(char *time_str, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    strftime(time_str, size, "%Y/%m/%d %I:%M:%S%p", t);
}

void send_all(int sock, const char *data) {
    send(sock, data, strlen(data), 0);
}

void broadcast_message(int sender_sock, const char *sender_id, const char *message) {
    char time_str[100];
    char send_buffer[BUFFER_SIZE + 200];

    get_time_string(time_str, sizeof(time_str));

    snprintf(send_buffer, sizeof(send_buffer),
             "%s %s: %s", time_str, sender_id, message);

    pthread_mutex_lock(&lock);

    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != sender_sock) {
            send_all(clients[i].socket, send_buffer);
        }
    }

    pthread_mutex_unlock(&lock);
}

void remove_client(int sock) {
    pthread_mutex_lock(&lock);

    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == sock) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&lock);
}

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    char client_id[100];

    send_all(client_sock, "Nhap client id theo cu phap: client_id: client_name\n");

    int n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (n <= 0) {
        close(client_sock);
        return NULL;
    }

    buffer[n] = '\0';

    if (strncmp(buffer, "client_id:", 10) != 0) {
        send_all(client_sock, "Sai cu phap. Vi du: client_id: abc\n");
        close(client_sock);
        return NULL;
    }

    strcpy(client_id, buffer + 10);

    client_id[strcspn(client_id, "\r\n")] = '\0';

    while (client_id[0] == ' ') {
        memmove(client_id, client_id + 1, strlen(client_id));
    }

    pthread_mutex_lock(&lock);

    if (client_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&lock);
        send_all(client_sock, "Server full\n");
        close(client_sock);
        return NULL;
    }

    clients[client_count].socket = client_sock;
    strcpy(clients[client_count].id, client_id);
    client_count++;

    pthread_mutex_unlock(&lock);

    send_all(client_sock, "Dang nhap thanh cong. Bat dau chat...\n");

    printf("Client %s connected\n", client_id);

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

        if (n <= 0) {
            break;
        }

        buffer[n] = '\0';

        broadcast_message(client_sock, client_id, buffer);
    }

    printf("Client %s disconnected\n", client_id);

    remove_client(client_sock);
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

    printf("Chat server running on port %d...\n", PORT);

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
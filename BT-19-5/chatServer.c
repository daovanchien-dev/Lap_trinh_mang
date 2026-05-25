#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>

#define PORT 5000
#define BUFFER_SIZE 1024

typedef struct {
    int client1;
    int client2;
} Pair;

int waiting_client = -1;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *chat_handler(void *arg) {
    Pair *pair = (Pair *)arg;

    int c1 = pair->client1;
    int c2 = pair->client2;

    char buffer[BUFFER_SIZE];

    send(c1, "Matched. You can chat now.\n", 27, 0);
    send(c2, "Matched. You can chat now.\n", 27, 0);

    fd_set readfds;
    int maxfd = (c1 > c2 ? c1 : c2) + 1;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(c1, &readfds);
        FD_SET(c2, &readfds);

        int activity = select(maxfd, &readfds, NULL, NULL, NULL);

        if (activity <= 0) {
            break;
        }

        if (FD_ISSET(c1, &readfds)) {
            int n = recv(c1, buffer, BUFFER_SIZE, 0);

            if (n <= 0) {
                break;
            }

            send(c2, buffer, n, 0);
        }

        if (FD_ISSET(c2, &readfds)) {
            int n = recv(c2, buffer, BUFFER_SIZE, 0);

            if (n <= 0) {
                break;
            }

            send(c1, buffer, n, 0);
        }
    }

    send(c1, "Partner disconnected. Closing chat.\n", 37, 0);
    send(c2, "Partner disconnected. Closing chat.\n", 37, 0);

    close(c1);
    close(c2);

    free(pair);
    return NULL;
}

int main() {
    int server_fd, client_socket;
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
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        return 1;
    }

    printf("Chat server running on port %d...\n", PORT);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        printf("Client connected\n");

        pthread_mutex_lock(&lock);

        if (waiting_client == -1) {
            waiting_client = client_socket;
            send(client_socket, "Waiting for another client...\n", 30, 0);
        } else {
            Pair *pair = malloc(sizeof(Pair));
            pair->client1 = waiting_client;
            pair->client2 = client_socket;

            waiting_client = -1;

            pthread_t tid;
            pthread_create(&tid, NULL, chat_handler, pair);
            pthread_detach(tid);
        }

        pthread_mutex_unlock(&lock);
    }

    close(server_fd);
    return 0;
}
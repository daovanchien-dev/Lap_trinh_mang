#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8888
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

typedef struct {
    int sock;
    char id[50];
    char name[100];
    int authenticated;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
fd_set master_set, read_set;

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].sock = -1;
}

void add_client(int sock) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sock == -1) {
            clients[i].sock = sock;
            clients[i].authenticated = 0;
            client_count++;
            FD_SET(sock, &master_set);
            break;
        }
    }
}

void remove_client(int sock) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sock == sock) {
            close(sock);
            FD_CLR(sock, &master_set);
            clients[i].sock = -1;
            client_count--;
            break;
        }
    }
}

void broadcast(char *msg, int sender_sock) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sock != -1 && clients[i].sock != sender_sock && clients[i].authenticated) {
            send(clients[i].sock, msg, strlen(msg), 0);
        }
    }
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char msg[BUFFER_SIZE + 100];  // Tăng kích thước để tránh truncation
    int bytes;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, MAX_CLIENTS);

    FD_ZERO(&master_set);
    FD_SET(server_sock, &master_set);
    init_clients();

    printf("Chat server running on port %d\n", PORT);

    while (1) {
        read_set = master_set;
        select(FD_SETSIZE, &read_set, NULL, NULL, NULL);

        for (int i = 0; i <= FD_SETSIZE; i++) {
            if (FD_ISSET(i, &read_set)) {
                if (i == server_sock) {
                    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
                    add_client(client_sock);
                    send(client_sock, "Enter your name (format: client_id: client_name): ", 50, 0);
                } else {
                    bytes = recv(i, buffer, BUFFER_SIZE - 1, 0);
                    if (bytes <= 0) {
                        remove_client(i);
                    } else {
                        buffer[bytes] = '\0';
                        buffer[strcspn(buffer, "\r\n")] = 0;

                        int idx = -1;
                        for (int j = 0; j < MAX_CLIENTS; j++)
                            if (clients[j].sock == i) idx = j;

                        if (!clients[idx].authenticated) {
                            char *colon = strchr(buffer, ':');
                            if (colon && colon > buffer && strlen(colon + 1) > 1) {
                                *colon = '\0';
                                strcpy(clients[idx].id, buffer);
                                strcpy(clients[idx].name, colon + 1);
                                clients[idx].authenticated = 1;
                                send(i, "OK! You are now in chat.\n", 25, 0);
                            } else {
                                send(i, "Invalid format! Use: client_id: client_name\n", 45, 0);
                            }
                        } else {
                            // Dùng strcpy + strcat thay vì snprintf để tránh warning
                            strcpy(msg, clients[idx].id);
                            strcat(msg, ": ");
                            strcat(msg, buffer);
                            strcat(msg, "\n");  
                            broadcast(msg, i);
                        }
                    }
                }
            }
        }
    }
    return 0;
}
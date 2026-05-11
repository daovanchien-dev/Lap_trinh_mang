#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080

void *handle_client(void *arg) {
    int client = *(int *)arg;
    free(arg);
    
    printf("New client connected: %d\n", client);
    
    char buf[256];
    int ret = recv(client, buf, sizeof(buf) - 1, 0);
    if (ret > 0) {
        buf[ret] = 0;
        puts(buf);
    }
    
    char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Xin chao cac ban</h1></body></html>";
    send(client, msg, strlen(msg), 0);
    
    close(client);
    return NULL;
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    if (listen(listener, 5) < 0) {
        perror("listen");
        exit(1);
    }
    
    printf("Server running on port %d\n", PORT);
    
    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;
        
        int *client_fd = malloc(sizeof(int));
        *client_fd = client;
        
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_fd);
        pthread_detach(thread);
    }
    
    close(listener);
    return 0;
}
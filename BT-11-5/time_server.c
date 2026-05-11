#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 9090
#define BUFFER_SIZE 256

void get_time(char *format, char *result) {
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);
    
    if (strcmp(format, "dd/mm/yyyy") == 0) {
        strftime(result, BUFFER_SIZE, "%d/%m/%Y", info);
    }
    else if (strcmp(format, "dd/mm/yy") == 0) {
        strftime(result, BUFFER_SIZE, "%d/%m/%y", info);
    }
    else if (strcmp(format, "mm/dd/yyyy") == 0) {
        strftime(result, BUFFER_SIZE, "%m/%d/%Y", info);
    }
    else if (strcmp(format, "mm/dd/yy") == 0) {
        strftime(result, BUFFER_SIZE, "%m/%d/%y", info);
    }
    else {
        strcpy(result, "ERROR: Invalid format");
    }
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    char format[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return NULL;
    }
    
    buffer[n] = '\0';
    buffer[strcspn(buffer, "\n")] = '\0';
    buffer[strcspn(buffer, "\r")] = '\0';
    
    sscanf(buffer, "%s %s", command, format);
    
    if (strcmp(command, "GET_TIME") == 0) {
        get_time(format, response);
    }
    else {
        strcpy(response, "ERROR: Invalid command");
    }
    
    send(client_fd, response, strlen(response), 0);
    close(client_fd);
    
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;
    
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
    
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(1);
    }
    
    printf("Time server running on port %d\n", PORT);
    
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_fd_ptr);
        pthread_detach(thread);
    }
    
    close(server_fd);
    return 0;
}
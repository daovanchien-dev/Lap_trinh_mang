#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<fcntl.h>

#define BUFFER_SIZE 1024
#define BACKLOG 5

int main(int argc, char* argv[]){
    if(argc != 4){
        fprintf(stderr, " Usage: %s <port> <greeting file> <out file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    char *greeting_file = argv[2];
    char *output_file = argv[3];

    // lay loi chao tu file
    FILE* greeting_fp = fopen(greeting_file, "r");
    if(greeting_fp == NULL){
        perror("Cannot open greeting file");
        exit(EXIT_FAILURE);
    }

    char greeting[BUFFER_SIZE];
    if(fgets(greeting, BUFFER_SIZE, greeting_fp) == NULL){
        fprintf(stderr, "Greeting file is empty\n");
        fclose(greeting_fp);
        exit(EXIT_FAILURE);
    }

    fclose(greeting_fp);

    // bo ky tu xuong dong
    greeting[strcspn(greeting, "\n")] = 0;

    // Tao socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // cho su dung lai dia chi
    int opt = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1){
        perror("Setsockopt failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // bind
    if(bind(server_socket,(struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Doi ket noi tu client
    if(listen(server_socket, BACKLOG) == -1){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // lay output tu file
    FILE* output_fp = fopen(output_file, "a");
    if(output_fp == NULL){
        perror("Cannot open output file");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    while(1){
        // Chap nhan ket noi tu client
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if(client_socket == -1){
            perror("Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        printf("Client connect from %s: %d\n", client_ip, ntohs(client_addr.sin_port));

        size_t greeting_len = strlen(greeting);
        if(send(client_socket, greeting, greeting_len, 0) == -1){
            perror("Send greeting failed");
            close(client_socket);
            continue;
        }

        if(send(client_socket, "\n", 1, 0) == -1){
            perror("Send newline failed");
            close(client_socket);
            continue;
        }

        // doc data tu client
        char buffer[BUFFER_SIZE];
        ssize_t bytes_received;

        while((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0){
            buffer[bytes_received] = '\0';
            fprintf(output_fp, "%s", buffer);
            fflush(output_fp);
            printf("Received from %s: %s", client_ip, buffer);
        }

        if(bytes_received == -1){
            perror("Receive failed");
        }

        close(client_socket);
        printf("Client %s disconnected\n", client_ip);

    }

    fclose(output_fp);
    close(server_socket);

    return 0;
}
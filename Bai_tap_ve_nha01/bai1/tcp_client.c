#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Use: %s <IP address> <Port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* server_ip = argv[1];
    int port = atoi(argv[2]);

    // tao socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket == -1){
        perror("Socket create failed");
        exit(EXIT_FAILURE);
    }

    // thiet lap dia chi server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    // tao ket noi toi server
    if(connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
        perror("Connection failed");

        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server %s: %d\n", server_ip, port);
    printf("Enter messages to send (type 'quit' to exit): \n");

    char buffer[BUFFER_SIZE];

    while(1){
        printf("> ");
        if(fgets(buffer, BUFFER_SIZE, stdin) == NULL){
            break;
        }

        // xoa ky tu xuong dong
        buffer[strcspn(buffer, "\n")] == 0;

        if(strcmp(buffer, "quit") == 0){
            break;
        }

        // gui du lieu
        size_t len = strlen(buffer);
        if(send(client_socket, buffer, len, 0) == -1){
            perror("Send failed");
            break;
        }
        if(send(client_socket, "\n", 1, 0) == -1){
            perror("Send newline failed");
            break;
        }

        printf("Sent: %s\n", buffer);
        
    }

    close(client_socket);
    printf("Connection closed\n");

    return 0;
}
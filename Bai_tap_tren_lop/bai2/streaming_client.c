#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server IP> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    
    // Tạo socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Cấu hình địa chỉ server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    // Kết nối đến server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server %s:%d\n", server_ip, port);
    printf("Enter data to send (Ctrl+D to exit):\n");
    
    char buffer[BUFFER_SIZE];
    while (1) {
        printf("> ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        // Xóa ký tự xuống dòng
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }
        
        if (len == 0) {
            continue;
        }
        
        // Gửi dữ liệu
        if (send(client_socket, buffer, len, 0) == -1) {
            perror("Send failed");
            break;
        }
    }
    
    close(client_socket);
    printf("Connection closed\n");
    
    return 0;
}
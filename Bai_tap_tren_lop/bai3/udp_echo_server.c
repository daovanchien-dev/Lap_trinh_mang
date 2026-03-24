#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define PORT 8080

int main(int argc, char *argv[]) {
    int port = PORT;
    
    // Kiểm tra tham số dòng lệnh
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number\n");
            exit(EXIT_FAILURE);
        }
    }
    
    // Tạo socket UDP
    int server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Cấu hình địa chỉ server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("UDP Echo Server listening on port %d\n", port);
    printf("Waiting for messages...\n");
    printf("Press Ctrl+C to exit\n\n");
    
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int message_count = 0;
    
    while (1) {
        // Nhận dữ liệu từ client
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recvfrom(server_socket, buffer, BUFFER_SIZE - 1, 0,
                                          (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (bytes_received == -1) {
            perror("Receive failed");
            continue;
        }
        
        message_count++;
        
        // Lấy thông tin client
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);
        
        // Hiển thị dữ liệu nhận được
        printf("[Message #%d] From %s:%d\n", message_count, client_ip, client_port);
        printf("Received: %s\n", buffer);
        
        // Echo lại dữ liệu cho client
        ssize_t bytes_sent = sendto(server_socket, buffer, bytes_received, 0,
                                    (struct sockaddr *)&client_addr, client_addr_len);
        
        if (bytes_sent == -1) {
            perror("Send failed");
        } else {
            printf("Echoed back: %s\n", buffer);
        }
        
        printf("\n");
    }
    
    close(server_socket);
    return 0;
}
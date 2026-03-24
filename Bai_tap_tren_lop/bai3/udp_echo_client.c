#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <server IP> [port]\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 8080\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *server_ip = argv[1];
    int port = 8080;  // Port mặc định
    
    if (argc == 3) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number\n");
            exit(EXIT_FAILURE);
        }
    }
    
    // Tạo socket UDP
    int client_socket = socket(AF_INET, SOCK_DGRAM, 0);
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
    
    printf("UDP Echo Client\n");
    printf("Server: %s:%d\n", server_ip, port);
    printf("Enter messages to send (type 'quit' to exit):\n");
    printf("\n");
    
    char buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_addr_len = sizeof(from_addr);
    int message_count = 0;
    
    // Thiết lập timeout cho recvfrom (5 giây)
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        perror("Setsockopt failed");
    }
    
    while (1) {
        printf("\n[%d] Enter message: ", ++message_count);
        
        // Đọc dữ liệu từ bàn phím
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        // Xóa ký tự xuống dòng
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }
        
        // Kiểm tra lệnh thoát
        if (strcmp(buffer, "quit") == 0) {
            printf("Exiting...\n");
            break;
        }
        
        if (len == 0) {
            message_count--;
            continue;
        }
        
        // Gửi dữ liệu đến server
        ssize_t bytes_sent = sendto(client_socket, buffer, len, 0,
                                    (struct sockaddr *)&server_addr, sizeof(server_addr));
        
        if (bytes_sent == -1) {
            perror("Send failed");
            continue;
        }
        
        printf("Sent: %s\n", buffer);
        
        // Nhận phản hồi từ server
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recvfrom(client_socket, buffer, BUFFER_SIZE - 1, 0,
                                          (struct sockaddr *)&from_addr, &from_addr_len);
        
        if (bytes_received == -1) {
            perror("Receive failed (timeout or error)");
            printf("Server may be down or unreachable\n");
            continue;
        }
        
        // Kiểm tra xem phản hồi có đúng từ server không
        if (from_addr.sin_addr.s_addr == server_addr.sin_addr.s_addr &&
            from_addr.sin_port == server_addr.sin_port) {
            printf("Echo reply: %s\n", buffer);
        } else {
            printf("Received from unknown source: %s\n", buffer);
        }
    }
    
    close(client_socket);
    printf("\nConnection closed\n");
    
    return 0;
}
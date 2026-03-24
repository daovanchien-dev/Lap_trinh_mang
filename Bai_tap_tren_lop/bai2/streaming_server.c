#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define BACKLOG 5
#define PATTERN "0123456789"
#define PATTERN_LEN 10

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    
    // Tạo socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Cho phép reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Setsockopt failed");
        close(server_socket);
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
    
    // Lắng nghe kết nối
    if (listen(server_socket, BACKLOG) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d\n", port);
    printf("Counting pattern: \"%s\"\n\n", PATTERN);
    
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    // Chấp nhận kết nối từ client
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket == -1) {
        perror("Accept failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("Client connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
    printf("\n\n");
    
    // Buffer lưu phần dư từ lần nhận trước
    char leftover[PATTERN_LEN];
    int leftover_len = 0;
    int total_count = 0;
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    
    // Nhận dữ liệu từ client
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        // Tạo buffer tạm để kết hợp phần dư và dữ liệu mới
        char temp[BUFFER_SIZE + PATTERN_LEN];
        int temp_len = 0;
        
        // Thêm phần dư từ lần trước
        if (leftover_len > 0) {
            memcpy(temp, leftover, leftover_len);
            temp_len = leftover_len;
        }
        
        // Thêm dữ liệu mới
        memcpy(temp + temp_len, buffer, bytes_received);
        temp_len += bytes_received;
        
        // Đếm số lần xuất hiện của pattern trong temp
        int count = 0;
        for (int i = 0; i <= temp_len - PATTERN_LEN; i++) {
            if (memcmp(temp + i, PATTERN, PATTERN_LEN) == 0) {
                count++;
                i += PATTERN_LEN - 1; // Nhảy qua pattern vừa tìm thấy
            }
        }
        
        // Cập nhật tổng số
        total_count += count;
        
        // Lưu phần dư cho lần sau (tối đa PATTERN_LEN-1 ký tự cuối)
        if (temp_len >= PATTERN_LEN - 1) {
            leftover_len = PATTERN_LEN - 1;
            memcpy(leftover, temp + temp_len - (PATTERN_LEN - 1), PATTERN_LEN - 1);
        } else {
            leftover_len = temp_len;
            memcpy(leftover, temp, temp_len);
        }
        
        // In ra màn hình số lần xuất hiện
        printf("Received data chunk\n");
        printf("Found %d occurrence(s) in this chunk\n", count);
        printf("Total occurrences so far: %d\n\n", total_count);
    }
    
    if (bytes_received == -1) {
        perror("Receive failed");
    }
    
    printf("\n");
    printf("Client disconnected\n");
    printf("FINAL RESULT: Total occurrences: %d\n", total_count);
    printf("\n");
    
    close(client_socket);
    close(server_socket);
    
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BACKLOG 5
#define MAX_PATH 512
#define MAX_FILENAME 256
#define BUFFER_SIZE 1024

// Hàm nhận chính xác số byte từ socket
ssize_t recv_exact(int sockfd, void *buffer, size_t len) {
    size_t total_received = 0;
    ssize_t bytes_received;
    
    while (total_received < len) {
        bytes_received = recv(sockfd, (char *)buffer + total_received, 
                             len - total_received, 0);
        if (bytes_received <= 0) {
            return bytes_received;
        }
        total_received += bytes_received;
    }
    return total_received;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    
    // Kiểm tra port hợp lệ
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        exit(EXIT_FAILURE);
    }
    
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
    printf("Waiting for client connections...\n");
    
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    while (1) {
        // Chấp nhận kết nối
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("\n");
        printf("Client connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        
        // NHẬN DỮ LIỆU 
        
        // Nhận độ dài đường dẫn
        int dir_len_net;
        if (recv_exact(client_socket, &dir_len_net, sizeof(int)) <= 0) {
            fprintf(stderr, "Failed to receive directory length\n");
            close(client_socket);
            continue;
        }
        int dir_len = ntohl(dir_len_net);
        
        // Nhận đường dẫn
        char current_dir[MAX_PATH];
        if (recv_exact(client_socket, current_dir, dir_len) <= 0) {
            fprintf(stderr, "Failed to receive directory path\n");
            close(client_socket);
            continue;
        }
        current_dir[dir_len] = '\0';
        
        // Nhận số lượng file
        int file_count_net;
        if (recv_exact(client_socket, &file_count_net, sizeof(int)) <= 0) {
            fprintf(stderr, "Failed to receive file count\n");
            close(client_socket);
            continue;
        }
        int file_count = ntohl(file_count_net);
        
        printf("\n--- Received Information ---\n");
        printf("Current Directory: %s\n", current_dir);
        printf("Number of files: %d\n\n", file_count);
        
        if (file_count > 0) {
            printf("Files:\n");
        }
        
        // Nhận thông tin từng file
        for (int i = 0; i < file_count; i++) {
            // Nhận độ dài tên file
            int name_len_net;
            if (recv_exact(client_socket, &name_len_net, sizeof(int)) <= 0) {
                fprintf(stderr, "Failed to receive filename length for file %d\n", i+1);
                break;
            }
            int name_len = ntohl(name_len_net);
            
            // Nhận tên file
            char filename[MAX_FILENAME];
            if (recv_exact(client_socket, filename, name_len) <= 0) {
                fprintf(stderr, "Failed to receive filename for file %d\n", i+1);
                break;
            }
            filename[name_len] = '\0';
            
            // Nhận kích thước file (int)
            int file_size_net;
            if (recv_exact(client_socket, &file_size_net, sizeof(int)) <= 0) {
                fprintf(stderr, "Failed to receive file size for file %d\n", i+1);
                break;
            }
            int file_size = ntohl(file_size_net);
            
            printf("  %s - %d bytes\n", filename, file_size);
        }
        
        printf("\n");
        
        close(client_socket);
        printf("Client disconnected\n");
    }
    
    close(server_socket);
    return 0;
}
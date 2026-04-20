#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>
#include <ctype.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define PORT 8888

typedef struct {
    int fd;
    char client_id[50];
    char client_name[100];
    int authenticated;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;

// Hàm lấy thời gian hiện tại dạng string
void get_current_time(char *buffer, size_t size) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, size, "%Y/%m/%d %H:%M:%S", timeinfo);
}

// Tìm client theo fd
int find_client_by_fd(int fd) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

// Xóa client khỏi danh sách
void remove_client(int index) {
    if (index < 0 || index >= client_count) return;
    
    close(clients[index].fd);
    
    for (int i = index; i < client_count - 1; i++) {
        clients[i] = clients[i + 1];
    }
    client_count--;
}

// Gửi tin nhắn đến tất cả client khác
void broadcast_message(int sender_fd, const char *message) {
    char time_str[64];
    get_current_time(time_str, sizeof(time_str));
    
    char full_message[BUFFER_SIZE];
    snprintf(full_message, sizeof(full_message), "%s %s", time_str, message);
    
    for (int i = 0; i < client_count; i++) {
        if (clients[i].fd != sender_fd && clients[i].authenticated) {
            send(clients[i].fd, full_message, strlen(full_message), 0);
        }
    }
}

// Xử lý tin nhắn từ client
void handle_client_message(int client_fd) {
    char buffer[BUFFER_SIZE];
    int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (n <= 0) {
        // Client ngắt kết nối
        int index = find_client_by_fd(client_fd);
        if (index >= 0) {
            printf("Client %s disconnected\n", clients[index].client_id);
            // Thông báo cho các client khác
            char leave_msg[BUFFER_SIZE];
            snprintf(leave_msg, sizeof(leave_msg), 
                     "*** %s has left the chat ***\n", 
                     clients[index].client_id);
            broadcast_message(client_fd, leave_msg);
            remove_client(index);
        }
        return;
    }
    
    buffer[n] = '\0';
    // Xóa ký tự newline nếu có
    buffer[strcspn(buffer, "\r\n")] = 0;
    
    int client_index = find_client_by_fd(client_fd);
    if (client_index < 0) return;
    
    if (!clients[client_index].authenticated) {
        // Kiểm tra cú pháp: "client_id: client_name"
        char *colon = strchr(buffer, ':');
        if (colon != NULL && colon > buffer && *(colon + 1) == ' ') {
            // Tách client_id và client_name
            int id_len = colon - buffer;
            if (id_len > 0 && id_len < 50) {
                strncpy(clients[client_index].client_id, buffer, id_len);
                clients[client_index].client_id[id_len] = '\0';
                
                strcpy(clients[client_index].client_name, colon + 2);
                
                // Kiểm tra client_name không rỗng và chỉ chứa chữ cái/số
                int valid = 1;
                if (strlen(clients[client_index].client_name) == 0) {
                    valid = 0;
                }
                for (int i = 0; clients[client_index].client_name[i] && valid; i++) {
                    if (!isalnum(clients[client_index].client_name[i])) {
                        valid = 0;
                        break;
                    }
                }
                
                if (valid) {
                    clients[client_index].authenticated = 1;
                    char welcome[BUFFER_SIZE];
                    snprintf(welcome, sizeof(welcome), 
                             "Welcome %s! You are now connected.\n", 
                             clients[client_index].client_id);
                    send(client_fd, welcome, strlen(welcome), 0);
                    
                    printf("Client authenticated: %s (%s)\n", 
                           clients[client_index].client_id, 
                           clients[client_index].client_name);
                    
                    // Thông báo cho các client khác
                    char join_msg[BUFFER_SIZE];
                    snprintf(join_msg, sizeof(join_msg), 
                             "*** %s has joined the chat ***\n", 
                             clients[client_index].client_id);
                    broadcast_message(client_fd, join_msg);
                } else {
                    send(client_fd, "Invalid name! Use format: client_id: client_name (name must be alphanumeric)\n", 71, 0);
                }
            } else {
                send(client_fd, "Client ID too long! Maximum 49 characters.\n", 43, 0);
            }
        } else {
            send(client_fd, "Invalid format! Please use: client_id: client_name\n", 51, 0);
        }
    } else {
        // Client đã xác thực, gửi tin nhắn đến các client khác
        if (strlen(buffer) > 0) {
            char message[BUFFER_SIZE + 100];
            snprintf(message, sizeof(message), "%s: %s\n", 
                     clients[client_index].client_id, buffer);
            broadcast_message(client_fd, message);
            
            // In ra server log
            printf("[%s] %s\n", clients[client_index].client_id, buffer);
        }
    }
}

int main(int argc, char *argv[]) {
    int server_fd, opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int port = PORT;
    
    // Kiểm tra tham số dòng lệnh
    if (argc == 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            printf("Invalid port number. Using default port %d\n", PORT);
            port = PORT;
        }
    }
    
    // Tạo socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket option để tái sử dụng địa chỉ
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("\n");
    printf("Chat Server running on port %d\n", port);
    printf("Waiting for connections...\n");
    printf("\n");
    
    // Mảng pollfd
    struct pollfd *fds = malloc((MAX_CLIENTS + 1) * sizeof(struct pollfd));
    int nfds = 1;
    
    // Thêm server socket vào mảng poll
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    
    // Khởi tạo mảng clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].authenticated = 0;
    }
    
    while (1) {
        int activity = poll(fds, nfds, -1);
        
        if (activity < 0) {
            perror("poll error");
            break;
        }
        
        // Kiểm tra kết nối mới
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket;
            
            if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
                perror("accept");
                continue;
            }
            
            printf("New connection from %s:%d\n", 
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            // Thêm client mới vào danh sách
            if (client_count < MAX_CLIENTS) {
                clients[client_count].fd = new_socket;
                clients[client_count].authenticated = 0;
                clients[client_count].client_id[0] = '\0';
                clients[client_count].client_name[0] = '\0';
                
                // Thêm vào mảng poll
                fds[nfds].fd = new_socket;
                fds[nfds].events = POLLIN;
                nfds++;
                client_count++;
                
                // Gửi yêu cầu nhập tên
                char *msg = "Please enter your identification in format: client_id: client_name\n";
                send(new_socket, msg, strlen(msg), 0);
            } else {
                printf("Max clients reached. Rejecting connection.\n");
                char *msg = "Server is full. Try again later.\n";
                send(new_socket, msg, strlen(msg), 0);
                close(new_socket);
            }
        }
        
        // Kiểm tra dữ liệu từ các client
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                handle_client_message(fds[i].fd);
                
                // Kiểm tra xem client còn tồn tại không (có thể đã bị xóa trong handle)
                int exists = 0;
                for (int j = 0; j < client_count; j++) {
                    if (clients[j].fd == fds[i].fd) {
                        exists = 1;
                        break;
                    }
                }
                
                // Nếu client đã bị xóa, cập nhật lại mảng poll
                if (!exists) {
                    close(fds[i].fd);
                    // Xóa fd khỏi mảng poll
                    for (int j = i; j < nfds - 1; j++) {
                        fds[j] = fds[j + 1];
                    }
                    nfds--;
                    i--; // Điều chỉnh index
                }
            }
        }
    }
    
    close(server_fd);
    free(fds);
    return 0;
}
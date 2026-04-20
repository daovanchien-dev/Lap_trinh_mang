#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <ctype.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define PORT 23
#define USER_PASS_FILE "users.txt"

typedef struct {
    int fd;
    int authenticated;
    char username[50];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;

// Cấu trúc lưu thông tin user/pass
typedef struct {
    char username[50];
    char password[50];
} User;

User users[100];
int user_count = 0;

// Hàm đọc file users.txt
int load_users(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Warning: Cannot open %s. Creating default users.\n", filename);
        // Tạo file mặc định nếu chưa có
        file = fopen(filename, "w");
        if (file) {
            fprintf(file, "admin admin\n");
            fprintf(file, "guest guest\n");
            fprintf(file, "user 123456\n");
            fclose(file);
        }
        file = fopen(filename, "r");
        if (!file) return 0;
    }
    
    user_count = 0;
    while (fscanf(file, "%49s %49s", users[user_count].username, users[user_count].password) == 2) {
        user_count++;
        if (user_count >= 100) break;
    }
    
    fclose(file);
    printf("Loaded %d users from %s\n", user_count, filename);
    return user_count;
}

// Kiểm tra đăng nhập
int authenticate(const char *username, const char *password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && 
            strcmp(users[i].password, password) == 0) {
            return 1;
        }
    }
    return 0;
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

// Xóa client
void remove_client(int index) {
    if (index < 0 || index >= client_count) return;
    
    printf("Client %s (fd=%d) disconnected\n", 
           clients[index].username[0] ? clients[index].username : "unknown",
           clients[index].fd);
    
    close(clients[index].fd);
    
    for (int i = index; i < client_count - 1; i++) {
        clients[i] = clients[i + 1];
    }
    client_count--;
}

// Thực thi lệnh và trả kết quả
void execute_command(int client_fd, const char *command) {
    char output_file[100];
    char system_cmd[BUFFER_SIZE];
    char result[BUFFER_SIZE];
    
    // Tạo tên file tạm
    snprintf(output_file, sizeof(output_file), "/tmp/cmd_output_%d.txt", client_fd);
    
    // Thực thi lệnh và ghi vào file
    snprintf(system_cmd, sizeof(system_cmd), "%s > %s 2>&1", command, output_file);
    
    printf("Executing: %s\n", system_cmd);
    int ret = system(system_cmd);
    
    // Đọc kết quả từ file
    FILE *fp = fopen(output_file, "r");
    if (fp) {
        char line[BUFFER_SIZE];
        int has_output = 0;
        
        // Gửi kết quả từng dòng
        while (fgets(line, sizeof(line), fp)) {
            send(client_fd, line, strlen(line), 0);
            has_output = 1;
        }
        fclose(fp);
        
        if (!has_output) {
            char *msg = "Command executed successfully (no output)\n";
            send(client_fd, msg, strlen(msg), 0);
        }
        
        // Xóa file tạm
        unlink(output_file);
    } else {
        char error_msg[BUFFER_SIZE];
        snprintf(error_msg, sizeof(error_msg), "Error: Cannot execute command or read output\n");
        send(client_fd, error_msg, strlen(error_msg), 0);
    }
    
    // Gửi prompt mới
    send(client_fd, "$ ", 2, 0);
}

// Xử lý tin nhắn từ client
void handle_client_message(int client_fd) {
    char buffer[BUFFER_SIZE];
    int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (n <= 0) {
        int index = find_client_by_fd(client_fd);
        if (index >= 0) {
            remove_client(index);
        }
        return;
    }
    
    buffer[n] = '\0';
    // Xóa ký tự newline và carriage return
    buffer[strcspn(buffer, "\r\n")] = 0;
    
    // Bỏ qua dòng trống
    if (strlen(buffer) == 0) {
        return;
    }
    
    int client_index = find_client_by_fd(client_fd);
    if (client_index < 0) return;
    
    if (!clients[client_index].authenticated) {
        // Chưa đăng nhập: xử lý user/pass
        char username[50], password[50];
        
        // Kiểm tra format "username password"
        if (sscanf(buffer, "%49s %49s", username, password) == 2) {
            if (authenticate(username, password)) {
                clients[client_index].authenticated = 1;
                strcpy(clients[client_index].username, username);
                
                char welcome_msg[BUFFER_SIZE];
                snprintf(welcome_msg, sizeof(welcome_msg), 
                         "\n\n"
                         "Welcome %s! Authentication successful.\n"
                         "You can now execute commands.\n"
                         "Type 'exit' or 'quit' to disconnect.\n"
                         "\n"
                         "$ ", username);
                send(client_fd, welcome_msg, strlen(welcome_msg), 0);
                
                printf("User %s authenticated (fd=%d)\n", username, client_fd);
            } else {
                char *msg = "Authentication failed! Invalid username or password.\n"
                            "Please login with: username password\n";
                send(client_fd, msg, strlen(msg), 0);
            }
        } else {
            char *msg = "Please login with: username password\n";
            send(client_fd, msg, strlen(msg), 0);
        }
    } else {
        // Đã đăng nhập: xử lý lệnh
        if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
            char *msg = "Goodbye!\n";
            send(client_fd, msg, strlen(msg), 0);
            remove_client(client_index);
            return;
        }
        
        if (strlen(buffer) > 0) {
            execute_command(client_fd, buffer);
        } else {
            send(client_fd, "$ ", 2, 0);
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
            printf("Invalid port. Using default port %d\n", PORT);
            port = PORT;
        }
    }
    
    // Load users từ file
    if (!load_users(USER_PASS_FILE)) {
        printf("Error: No users loaded. Creating default users.\n");
    }
    
    // Tạo socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket option
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("\n\n");
    printf("TELNET SERVER\n");
    printf("Port: %d\n", port);
    printf("Users file: %s\n", USER_PASS_FILE);
    printf("Loaded %d users\n", user_count);
    printf("\n");
    printf("Server is running. Waiting for connections...\n");
    printf("Use 'telnet localhost %d' to connect\n", port);
    printf("Or 'nc localhost %d'\n", port);
    printf("\n");
    
    // Mảng pollfd
    struct pollfd *fds = malloc((MAX_CLIENTS + 1) * sizeof(struct pollfd));
    int nfds = 1;
    
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    
    // Khởi tạo clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].authenticated = 0;
        clients[i].username[0] = '\0';
    }
    
    // Gửi banner chào mừng
    char banner[] = "\n\n"
                    "Welcome to Telnet Server\n"
                    "Please login with: username password\n"
                    "\n"
                    "Login: ";
    
    while (1) {
        int activity = poll(fds, nfds, -1);
        
        if (activity < 0) {
            perror("poll error");
            break;
        }
        
        // Kết nối mới
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket;
            
            if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
                perror("accept");
                continue;
            }
            
            printf("New connection from %s:%d (fd=%d)\n", 
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                   new_socket);
            
            if (client_count < MAX_CLIENTS) {
                clients[client_count].fd = new_socket;
                clients[client_count].authenticated = 0;
                clients[client_count].username[0] = '\0';
                
                fds[nfds].fd = new_socket;
                fds[nfds].events = POLLIN;
                nfds++;
                client_count++;
                
                // Gửi banner đăng nhập
                send(new_socket, banner, strlen(banner), 0);
            } else {
                printf("Max clients reached. Rejecting connection.\n");
                char *msg = "Server is full. Try again later.\n";
                send(new_socket, msg, strlen(msg), 0);
                close(new_socket);
            }
        }
        
        // Dữ liệu từ client
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                handle_client_message(fds[i].fd);
                
                // Kiểm tra client còn tồn tại không
                int exists = 0;
                for (int j = 0; j < client_count; j++) {
                    if (clients[j].fd == fds[i].fd) {
                        exists = 1;
                        break;
                    }
                }
                
                if (!exists) {
                    close(fds[i].fd);
                    for (int j = i; j < nfds - 1; j++) {
                        fds[j] = fds[j + 1];
                    }
                    nfds--;
                    i--;
                }
            }
        }
    }
    
    close(server_fd);
    free(fds);
    return 0;
}
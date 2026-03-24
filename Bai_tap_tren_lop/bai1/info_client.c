#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_FILES 1000
#define MAX_PATH 512
#define MAX_FILENAME 256

// Cấu trúc đóng gói thông tin file - kích thước tối thiểu
typedef struct __attribute__((packed)) {
    char name[MAX_FILENAME];  // Tên file
    int size;                  // Kích thước file (int)
} FileInfo;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server IP> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    
    // Lấy thư mục hiện tại
    char current_dir[MAX_PATH];
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perror("getcwd failed");
        exit(EXIT_FAILURE);
    }
    
    // Mở thư mục hiện tại
    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("opendir failed");
        exit(EXIT_FAILURE);
    }
    
    // Đọc danh sách file
    FileInfo *file_list = NULL;
    int file_count = 0;
    struct dirent *entry;
    struct stat file_stat;
    
    // Đếm số lượng file trước
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (stat(entry->d_name, &file_stat) == 0) {
            if (S_ISREG(file_stat.st_mode)) {
                file_count++;
            }
        }
    }
    
    if (file_count == 0) {
        printf("No regular files found in current directory\n");
        closedir(dir);
        exit(EXIT_SUCCESS);
    }
    
    // Cấp phát bộ nhớ cho danh sách file
    file_list = (FileInfo *)malloc(file_count * sizeof(FileInfo));
    if (file_list == NULL) {
        perror("malloc failed");
        closedir(dir);
        exit(EXIT_FAILURE);
    }
    
    // Reset lại thư mục để đọc lại
    rewinddir(dir);
    
    // Lấy thông tin các file
    int index = 0;
    while ((entry = readdir(dir)) != NULL && index < file_count) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (stat(entry->d_name, &file_stat) == 0) {
            if (S_ISREG(file_stat.st_mode)) {
                strncpy(file_list[index].name, entry->d_name, MAX_FILENAME - 1);
                file_list[index].name[MAX_FILENAME - 1] = '\0';
                file_list[index].size = (int)file_stat.st_size;  // Chuyển thành int
                index++;
            }
        }
    }
    
    closedir(dir);
    
    // Tạo socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        free(file_list);
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
        free(file_list);
        exit(EXIT_FAILURE);
    }
    
    // Kết nối đến server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        free(file_list);
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server %s:%d\n", server_ip, port);
    printf("Current directory: %s\n", current_dir);
    printf("Found %d files\n", file_count);
    
    //  ĐÓNG GÓI DỮ LIỆU 
    // Gửi độ dài đường dẫn + đường dẫn
    int dir_len = strlen(current_dir);
    dir_len = htonl(dir_len);  // Chuyển sang network byte order
    send(client_socket, &dir_len, sizeof(int), 0);
    send(client_socket, current_dir, strlen(current_dir), 0);
    
    // Gửi số lượng file
    int file_count_net = htonl(file_count);
    send(client_socket, &file_count_net, sizeof(int), 0);
    
    // Gửi từng file: tên file + kích thước
    for (int i = 0; i < file_count; i++) {
        // Gửi độ dài tên file
        int name_len = strlen(file_list[i].name);
        name_len = htonl(name_len);
        send(client_socket, &name_len, sizeof(int), 0);
        
        // Gửi tên file
        send(client_socket, file_list[i].name, strlen(file_list[i].name), 0);
        
        // Gửi kích thước file (int)
        int file_size_net = htonl(file_list[i].size);
        send(client_socket, &file_size_net, sizeof(int), 0);
        
        printf("  Sent: %s - %d bytes\n", file_list[i].name, file_list[i].size);
    }
    
    printf("\nAll data sent successfully!\n");
    
    // Đóng kết nối
    close(client_socket);
    free(file_list);
    
    return 0;
}
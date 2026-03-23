#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 256

typedef struct {
    char mssv[20];
    char full_name[100];
    char birth_date[20];
    float gpa;
} Student;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server IP> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    
    // tạo socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // thiết lập địa chỉ
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    
    // yêu cầu kêt nối tới server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server %s:%d\n", server_ip, port);
    printf("Enter student information:\n");
    
    Student student;
    memset(&student, 0, sizeof(student)); 
    
    // Lấy thông tin sinh viên
    printf("MSSV: ");
    if (fgets(student.mssv, sizeof(student.mssv), stdin) == NULL) {
        fprintf(stderr, "Error reading MSSV\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    student.mssv[strcspn(student.mssv, "\n")] = 0;
    
    printf("Full name: ");
    if (fgets(student.full_name, sizeof(student.full_name), stdin) == NULL) {
        fprintf(stderr, "Error reading full name\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    student.full_name[strcspn(student.full_name, "\n")] = 0;
    
    printf("Birth date (YYYY-MM-DD): ");
    if (fgets(student.birth_date, sizeof(student.birth_date), stdin) == NULL) {
        fprintf(stderr, "Error reading birth date\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    student.birth_date[strcspn(student.birth_date, "\n")] = 0;
    
    printf("GPA: ");
    char gpa_str[BUFFER_SIZE];
    if (fgets(gpa_str, sizeof(gpa_str), stdin) == NULL) {
        fprintf(stderr, "Error reading GPA\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    student.gpa = atof(gpa_str);
    
    // Gửi dữ liệu
    if (send(client_socket, &student, sizeof(Student), 0) == -1) {
        perror("Send failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("Student information sent successfully!\n");
    
    close(client_socket);
    return 0;
}
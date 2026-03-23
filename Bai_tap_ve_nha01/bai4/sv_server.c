#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define BACKLOG 5

typedef struct {
    char mssv[20];
    char full_name[100];
    char birth_date[20];
    float gpa;
} Student;

void get_current_time(char *time_str, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(time_str, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <log file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    char *log_file = argv[2];
    
    // Create socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Allow reuse of address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Setsockopt failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    // Configure server address - zero out structure
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
    
    // Listen for connections
    if (listen(server_socket, BACKLOG) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d\n", port);
    printf("Logging to file: %s\n", log_file);
    
    // Open log file for appending
    FILE *log_fp = fopen(log_file, "a");
    if (log_fp == NULL) {
        perror("Cannot open log file");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    while (1) {
        // Accept client connection
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }
        
        // Get client IP address
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        // Receive student data
        Student student;
        memset(&student, 0, sizeof(student)); // Zero out student structure
        ssize_t bytes_received = recv(client_socket, &student, sizeof(Student), 0);
        
        if (bytes_received == sizeof(Student)) {
            // Get current time
            char current_time[30];
            get_current_time(current_time, sizeof(current_time));
            
            // Print to console
            printf("\n=== Student Information Received ===\n");
            printf("Client IP: %s\n", client_ip);
            printf("Time: %s\n", current_time);
            printf("MSSV: %s\n", student.mssv);
            printf("Full Name: %s\n", student.full_name);
            printf("Birth Date: %s\n", student.birth_date);
            printf("GPA: %.2f\n", student.gpa);
            printf("===================================\n\n");
            
            // Write to log file
            fprintf(log_fp, "%s %s %s %s %s %.2f\n", 
                    client_ip, current_time, student.mssv, 
                    student.full_name, student.birth_date, student.gpa);
            fflush(log_fp);
            
            printf("Data logged to %s\n", log_file);
        } else if (bytes_received == -1) {
            perror("Receive failed");
        } else {
            printf("Received incomplete data from %s (got %zd bytes, expected %zu)\n", 
                   client_ip, bytes_received, sizeof(Student));
        }
        
        close(client_socket);
    }
    
    fclose(log_fp);
    close(server_socket);
    
    return 0;
}
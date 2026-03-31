#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <ctype.h>

#define MAX_CLIENTS 10

typedef struct {
    int fd;
    int step; // 0: ask name, 1: ask mssv
    char name[100];
} Client;

Client clients[MAX_CLIENTS];

void remove_newline(char *s) {
    s[strcspn(s, "\n")] = 0;
}

// Hàm lấy họ viết tắt (chữ cái đầu của các từ, trừ từ cuối cùng)
void get_last_name_abbr(char *fullname, char *ten, char *abbr) {
    char *token;
    char name_copy[100];
    strcpy(name_copy, fullname);
    
    // Chuyển sang chữ thường
    for (int i = 0; name_copy[i]; i++) {
        name_copy[i] = tolower(name_copy[i]);
    }
    
    // Tách từng từ
    token = strtok(name_copy, " ");
    char words[10][50];
    int word_count = 0;
    
    while (token != NULL) {
        strcpy(words[word_count], token);
        word_count++;
        token = strtok(NULL, " ");
    }
    
    if (word_count == 0) {
        strcpy(ten, "");
        strcpy(abbr, "");
        return;
    }
    
    // Lấy tên (từ cuối cùng)
    strcpy(ten, words[word_count - 1]);
    
    // Lấy họ viết tắt (chữ cái đầu của các từ còn lại, trừ từ cuối)
    abbr[0] = '\0';
    for (int i = 0; i < word_count - 1; i++) {
        if (strlen(words[i]) > 0) {
            char first_char = words[i][0];
            char temp[2] = {first_char, '\0'};
            strcat(abbr, temp);
        }
    }
}

// tạo email theo format: Ten.ho_vi_tat_{2 số cuối năm}{đuôi MSSV}@sis.hust.edu.vn
void generate_email(char *name, char *mssv, char *email) {
    char ten[50] = "";
    char ho_abbr[50] = "";
    
    // Lấy tên và họ viết tắt
    get_last_name_abbr(name, ten, ho_abbr);
    
    // Lấy 2 số cuối của năm (4 số đầu của MSSV)
    char year[3] = "";
    char suffix[50] = "";
    
    int len = strlen(mssv);
    if (len >= 4) {
        // Lấy 2 số cuối của năm (ví dụ: 2022 -> 22)
        strncpy(year, mssv + 2, 2);
        year[2] = '\0';
        
        // Lấy phần đuôi còn lại (từ vị trí thứ 4 trở đi)
        if (len > 4) {
            strcpy(suffix, mssv + 4);
        }
    }
    
    // Tạo email: ten.ho_vi_tat_{year}{suffix}@sis.hust.edu.vn
    sprintf(email, "%s.%s%s%s@sis.hust.edu.vn", ten, ho_abbr, year, suffix);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    
    if (listener < 0) {
        perror("socket failed");
        return 1;
    }
    
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return 1;
    }
    
    if (listen(listener, 5) < 0) {
        perror("listen failed");
        return 1;
    }
    
    fd_set readfds;
    
    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;
    
    printf("Server started on port 9000...\n");
    printf("Waiting for connections...\n");
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listener, &readfds);
        int maxfd = listener;
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                FD_SET(clients[i].fd, &readfds);
                if (clients[i].fd > maxfd)
                    maxfd = clients[i].fd;
            }
        }
        
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select failed");
            break;
        }
        
        // có client mới
        if (FD_ISSET(listener, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client = accept(listener, (struct sockaddr*)&client_addr, &addr_len);
            
            if (client < 0) {
                perror("accept failed");
                continue;
            }
            
            int added = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) {
                    clients[i].fd = client;
                    clients[i].step = 0;
                    strcpy(clients[i].name, "");
                    
                    char welcome_msg[] = "Nhap ho ten: ";
                    send(client, welcome_msg, strlen(welcome_msg), 0);
                    
                    printf("New client connected (fd: %d)\n", client);
                    added = 1;
                    break;
                }
            }
            
            if (!added) {
                printf("Max clients reached, rejecting connection\n");
                const char *msg = "Server is full. Try again later.\n";
                send(client, msg, strlen(msg), 0);
                close(client);
            }
        }
        
        // xử lý client
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == -1) continue;
            
            if (FD_ISSET(clients[i].fd, &readfds)) {
                char buf[256];
                int n = recv(clients[i].fd, buf, sizeof(buf)-1, 0);
                
                if (n <= 0) {
                    printf("Client (fd: %d) disconnected\n", clients[i].fd);
                    close(clients[i].fd);
                    clients[i].fd = -1;
                    continue;
                }
                
                buf[n] = 0;
                remove_newline(buf);
                
                if (clients[i].step == 0) {
                    strcpy(clients[i].name, buf);
                    send(clients[i].fd, "Nhap MSSV: ", 12, 0);
                    clients[i].step = 1;
                    printf("Client (fd: %d) entered name: %s\n", clients[i].fd, buf);
                }
                else {
                    // Kiểm tra MSSV có phải là số và có ít nhất 4 chữ số không
                    int is_valid = 1;
                    int len = strlen(buf);
                    
                    if (len < 4) {
                        is_valid = 0;
                    } else {
                        for (int j = 0; j < len; j++) {
                            if (!isdigit(buf[j])) {
                                is_valid = 0;
                                break;
                            }
                        }
                    }
                    
                    if (is_valid) {
                        char email[200];
                        generate_email(clients[i].name, buf, email);
                        
                        char response[300];
                        sprintf(response, "Email cua ban la: %s\n", email);
                        send(clients[i].fd, response, strlen(response), 0);
                        
                        printf("Client (fd: %d) - Name: %s, MSSV: %s -> Email: %s\n", 
                               clients[i].fd, clients[i].name, buf, email);
                        
                        close(clients[i].fd);
                        clients[i].fd = -1;
                    } else {
                        const char *error_msg = "Loi: MSSV khong hop le (phai la so va co it nhat 4 chu so). Vui long nhap lai: ";
                        send(clients[i].fd, error_msg, strlen(error_msg), 0);
                    }
                }
            }
        }
    }
    
    // Cleanup
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1) {
            close(clients[i].fd);
        }
    }
    close(listener);
    
    return 0;
}
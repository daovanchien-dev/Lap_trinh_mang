#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#define PORT 8889
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024
#define PASS_FILE "pass.txt"

typedef struct {
    int sock;
    char username[50];
    int authenticated;
} Client;

Client clients[MAX_CLIENTS];
fd_set master_set, read_set;
int client_count = 0;

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].sock = -1;
        clients[i].authenticated = 0;
    }
}

void add_client(int sock) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sock == -1) {
            clients[i].sock = sock;
            clients[i].authenticated = 0;
            client_count++;
            FD_SET(sock, &master_set);
            break;
        }
    }
}

void remove_client(int sock) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sock == sock) {
            close(sock);
            FD_CLR(sock, &master_set);
            clients[i].sock = -1;
            clients[i].authenticated = 0;
            client_count--;
            break;
        }
    }
}

int check_authentication(char *user, char *pass) {
    FILE *fp = fopen(PASS_FILE, "r");
    if (!fp) {
        printf("Error: Cannot open %s\n", PASS_FILE);
        return 0;
    }
    
    char line[200];
    char file_user[50], file_pass[50];
    
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;
        
        // Skip empty lines
        if (strlen(line) == 0) continue;
        
        // Parse user and pass (separated by space or tab)
        char *space = strchr(line, ' ');
        if (!space) space = strchr(line, '\t');
        
        if (space) {
            *space = '\0';
            strcpy(file_user, line);
            strcpy(file_pass, space + 1);
            
            if (strcmp(user, file_user) == 0 && strcmp(pass, file_pass) == 0) {
                fclose(fp);
                return 1;
            }
        }
    }
    
    fclose(fp);
    return 0;
}

void execute_command(char *cmd, char *output) {
    char temp_file[] = "temp_output_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) {
        strcpy(output, "Error: Cannot create temp file\n");
        return;
    }
    close(fd);
    
    // Build command with output redirection
    char full_cmd[BUFFER_SIZE];
    snprintf(full_cmd, sizeof(full_cmd), "%s > %s 2>&1", cmd, temp_file);
    
    // Execute command
    system(full_cmd);
    
    // Read output from temp file
    FILE *fp = fopen(temp_file, "r");
    if (!fp) {
        strcpy(output, "Error: Cannot read output\n");
        unlink(temp_file);
        return;
    }
    
    output[0] = '\0';
    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), fp)) {
        strcat(output, line);
    }
    
    fclose(fp);
    unlink(temp_file);
    
    if (strlen(output) == 0) {
        strcpy(output, "Command executed successfully (no output)\n");
    }
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char output[BUFFER_SIZE * 10];
    int bytes;
    
    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        exit(1);
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    // Listen
    listen(server_sock, MAX_CLIENTS);
    
    FD_ZERO(&master_set);
    FD_SET(server_sock, &master_set);
    init_clients();
    
    printf("Telnet server running on port %d\n", PORT);
    printf("Authentication file: %s\n", PASS_FILE);
    
    while (1) {
        read_set = master_set;
        select(FD_SETSIZE, &read_set, NULL, NULL, NULL);
        
        for (int i = 0; i <= FD_SETSIZE; i++) {
            if (FD_ISSET(i, &read_set)) {
                if (i == server_sock) {
                    // New connection
                    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
                    add_client(client_sock);
                    send(client_sock, "TELNET SERVER\nLogin: ", 28, 0);
                    printf("New client connected from %s:%d\n", 
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                } else {
                    // Existing client
                    bytes = recv(i, buffer, BUFFER_SIZE - 1, 0);
                    if (bytes <= 0) {
                        // Client disconnected
                        remove_client(i);
                        printf("Client disconnected\n");
                    } else {
                        buffer[bytes] = '\0';
                        buffer[strcspn(buffer, "\r\n")] = 0;
                        
                        // Find client index
                        int idx = -1;
                        for (int j = 0; j < MAX_CLIENTS; j++)
                            if (clients[j].sock == i) idx = j;
                        
                        if (idx == -1) continue;
                        
                        if (!clients[idx].authenticated) {
                            // Authentication state
                            static int step = 0;
                            static char username[50];
                            
                            // Simple state management using send/recv pattern
                            if (step == 0) {
                                strcpy(username, buffer);
                                send(i, "Password: ", 10, 0);
                                step = 1;
                            } else if (step == 1) {
                                if (check_authentication(username, buffer)) {
                                    clients[idx].authenticated = 1;
                                    strcpy(clients[idx].username, username);
                                    send(i, "\nLOGIN SUCCESS\nAvailable commands: ls, dir, pwd, date, whoami, etc.\n> ", 68, 0);
                                    printf("User '%s' authenticated successfully\n", username);
                                } else {
                                    send(i, "\nLogin failed! Invalid username or password.\nLogin: ", 55, 0);
                                    step = 0;
                                }
                            }
                        } else {
                            // Execute command
                            if (strlen(buffer) == 0) {
                                send(i, "> ", 2, 0);
                                continue;
                            }
                            
                            if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
                                send(i, "Goodbye!\n", 9, 0);
                                remove_client(i);
                                printf("User '%s' logged out\n", clients[idx].username);
                                continue;
                            }
                            
                            // Execute command and get output
                            execute_command(buffer, output);
                            send(i, output, strlen(output), 0);
                            send(i, "> ", 2, 0);
                            
                            printf("User '%s' executed: %s\n", clients[idx].username, buffer);
                        }
                    }
                }
            }
        }
    }
    
    close(server_sock);
    return 0;
}
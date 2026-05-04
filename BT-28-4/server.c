#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
#define MAX_TOPICS 50
#define TOPIC_LEN 50

// Mỗi client có danh sách topic đã đăng ký
typedef struct {
    int sockfd;
    char topics[MAX_TOPICS][TOPIC_LEN];
    int topic_count;
} Client;

// Mỗi topic có danh sách client đang đăng ký
typedef struct {
    char name[TOPIC_LEN];
    int clients[MAX_CLIENTS];
    int client_count;
} Topic;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

Topic topics[MAX_TOPICS];
int topic_count = 0;
pthread_mutex_t topic_mutex = PTHREAD_MUTEX_INITIALIZER;

// Tìm client theo socket
int find_client(int sockfd) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sockfd == sockfd) return i;
    }
    return -1;
}

// Tìm topic, trả về index, nếu chưa có thì tạo mới
int find_or_create_topic(const char* topic_name) {
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) return i;
    }
    if (topic_count < MAX_TOPICS) {
        strcpy(topics[topic_count].name, topic_name);
        topics[topic_count].client_count = 0;
        return topic_count++;
    }
    return -1;
}

// Thêm client vào topic (đăng ký)
void subscribe(int client_sock, const char* topic_name) {
    pthread_mutex_lock(&topic_mutex);
    
    int topic_idx = find_or_create_topic(topic_name);
    if (topic_idx == -1) {
        pthread_mutex_unlock(&topic_mutex);
        send(client_sock, "ERROR: Too many topics\n", 23, 0);
        return;
    }
    
    Topic* t = &topics[topic_idx];
    // Kiểm tra đã đăng ký chưa
    for (int i = 0; i < t->client_count; i++) {
        if (t->clients[i] == client_sock) {
            pthread_mutex_unlock(&topic_mutex);
            send(client_sock, "Already subscribed\n", 19, 0);
            return;
        }
    }
    
    if (t->client_count < MAX_CLIENTS) {
        t->clients[t->client_count++] = client_sock;
        char msg[100];
        sprintf(msg, "SUBSCRIBED to %s\n", topic_name);
        send(client_sock, msg, strlen(msg), 0);
        printf("Client %d subscribed to [%s]\n", client_sock, topic_name);
    } else {
        send(client_sock, "ERROR: Topic full\n", 18, 0);
    }
    
    pthread_mutex_unlock(&topic_mutex);
}

// Xóa client khỏi topic (hủy đăng ký)
void unsubscribe(int client_sock, const char* topic_name) {
    pthread_mutex_lock(&topic_mutex);
    
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            Topic* t = &topics[i];
            for (int j = 0; j < t->client_count; j++) {
                if (t->clients[j] == client_sock) {
                    // Dịch mảng lên
                    for (int k = j; k < t->client_count - 1; k++) {
                        t->clients[k] = t->clients[k + 1];
                    }
                    t->client_count--;
                    char msg[100];
                    sprintf(msg, "UNSUBSCRIBED from %s\n", topic_name);
                    send(client_sock, msg, strlen(msg), 0);
                    printf("Client %d unsubscribed from [%s]\n", client_sock, topic_name);
                    pthread_mutex_unlock(&topic_mutex);
                    return;
                }
            }
            break;
        }
    }
    
    char msg[100];
    sprintf(msg, "Not subscribed to %s\n", topic_name);
    send(client_sock, msg, strlen(msg), 0);
    pthread_mutex_unlock(&topic_mutex);
}

// Gửi tin nhắn đến tất cả client đã đăng ký topic (trừ người gửi)
void publish(int sender_sock, const char* topic_name, const char* message) {
    pthread_mutex_lock(&topic_mutex);
    
    int found = 0;
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            found = 1;
            Topic* t = &topics[i];
            char full_msg[BUFFER_SIZE];
            snprintf(full_msg, BUFFER_SIZE, "[%s] %s\n", topic_name, message);
            
            printf("PUB to [%s]: %s (forwarding to %d clients)\n", 
                   topic_name, message, t->client_count);
            
            for (int j = 0; j < t->client_count; j++) {
                if (t->clients[j] != sender_sock) {
                    send(t->clients[j], full_msg, strlen(full_msg), 0);
                }
            }
            break;
        }
    }
    
    if (!found) {
        char msg[100];
        sprintf(msg, "Topic [%s] has no subscribers\n", topic_name);
        send(sender_sock, msg, strlen(msg), 0);
    }
    
    pthread_mutex_unlock(&topic_mutex);
}

// Xóa client khi ngắt kết nối (khỏi tất cả topic)
void remove_client_from_all_topics(int client_sock) {
    pthread_mutex_lock(&topic_mutex);
    for (int i = 0; i < topic_count; i++) {
        Topic* t = &topics[i];
        for (int j = 0; j < t->client_count; j++) {
            if (t->clients[j] == client_sock) {
                for (int k = j; k < t->client_count - 1; k++) {
                    t->clients[k] = t->clients[k + 1];
                }
                t->client_count--;
                break;
            }
        }
    }
    pthread_mutex_unlock(&topic_mutex);
}

// Xử lý từng client
void* handle_client(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int bytes;
    
    while ((bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        // Xóa ký tự xuống dòng
        buffer[strcspn(buffer, "\r\n")] = 0;
        
        char cmd[10], topic[TOPIC_LEN], msg[BUFFER_SIZE];
        
        // Xử lý lệnh
        if (sscanf(buffer, "%s %s", cmd, topic) == 2) {
            if (strcmp(cmd, "SUB") == 0) {
                subscribe(client_sock, topic);
            }
            else if (strcmp(cmd, "UNSUB") == 0) {
                unsubscribe(client_sock, topic);
            }
            else if (strcmp(cmd, "PUB") == 0) {
                // Lấy phần message (sau topic)
                char* space = strchr(buffer + 4, ' ');
                if (space != NULL) {
                    strcpy(msg, space + 1);
                    publish(client_sock, topic, msg);
                } else {
                    send(client_sock, "ERROR: Missing message\n", 23, 0);
                }
            }
            else {
                send(client_sock, "ERROR: Unknown command\n", 23, 0);
            }
        }
        else {
            send(client_sock, "ERROR: Invalid format. Use: SUB <topic> | UNSUB <topic> | PUB <topic> <msg>\n", 73, 0);
        }
    }
    
    // Client ngắt kết nối
    printf("Client %d disconnected\n", client_sock);
    remove_client_from_all_topics(client_sock);
    
    // Xóa khỏi danh sách clients
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sockfd == client_sock) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
    
    close(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Tạo socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Cấu hình địa chỉ server
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(1);
    }
    
    // Listen
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(1);
    }
    
    printf("\n");
    printf("Server running on port %d\n", PORT);
    printf("Waiting for connections...\n");
    printf("\n");
    
    // Chấp nhận kết nối
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        // Lưu client mới
        pthread_mutex_lock(&client_mutex);
        if (client_count < MAX_CLIENTS) {
            clients[client_count].sockfd = client_sock;
            clients[client_count].topic_count = 0;
            client_count++;
            printf("New client connected: socket %d\n", client_sock);
        } else {
            printf("Max clients reached, rejecting\n");
            close(client_sock);
            pthread_mutex_unlock(&client_mutex);
            continue;
        }
        pthread_mutex_unlock(&client_mutex);
        
        // Tạo thread xử lý client
        int* new_sock = malloc(sizeof(int));
        *new_sock = client_sock;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, new_sock);
        pthread_detach(tid);
    }
    
    close(server_sock);
    return 0;
}
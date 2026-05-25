#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>

#define PORT 5000
#define BUFFER_SIZE 1024
#define FOLDER "files"

void send_all(int sock, const char *data, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(sock, data + sent, len - sent, 0);
        if (n <= 0) break;
        sent += n;
    }
}

int recv_line(int sock, char *buffer, int size) {
    int i = 0;
    char c;

    while (i < size - 1) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) break;

        if (c == '\n') break;

        if (c != '\r') {
            buffer[i++] = c;
        }
    }

    buffer[i] = '\0';
    return i;
}

int is_regular_file(const char *path) {
    struct stat st;

    if (stat(path, &st) == -1) {
        return 0;
    }

    return S_ISREG(st.st_mode);
}

void handle_client(int client_sock) {
    DIR *dir;
    struct dirent *entry;

    char files[100][256];
    int count = 0;

    dir = opendir(FOLDER);

    if (dir == NULL) {
        send_all(client_sock, "ERROR no files to download\r\n", strlen("ERROR no files to download\r\n"));
        close(client_sock);
        exit(0);
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[512];

        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", FOLDER, entry->d_name);

        if (is_regular_file(path)) {
            strcpy(files[count], entry->d_name);
            count++;
        }
    }

    closedir(dir);

    if (count == 0) {
        send_all(client_sock, "ERROR no files to download\r\n", strlen("ERROR no files to download\r\n"));
        close(client_sock);
        exit(0);
    }

    char buffer[BUFFER_SIZE];

    snprintf(buffer, sizeof(buffer), "OK %d\r\n", count);
    send_all(client_sock, buffer, strlen(buffer));

    for (int i = 0; i < count; i++) {
        snprintf(buffer, sizeof(buffer), "%s\r\n", files[i]);
        send_all(client_sock, buffer, strlen(buffer));
    }

    send_all(client_sock, "\r\n", 2);

    while (1) {
        char filename[256];
        char filepath[512];

        int len = recv_line(client_sock, filename, sizeof(filename));

        if (len <= 0) {
            break;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", FOLDER, filename);

        if (is_regular_file(filepath)) {
            FILE *fp = fopen(filepath, "rb");

            if (fp == NULL) {
                send_all(client_sock, "ERROR file not found\r\n", strlen("ERROR file not found\r\n"));
                continue;
            }

            fseek(fp, 0, SEEK_END);
            long file_size = ftell(fp);
            rewind(fp);

            snprintf(buffer, sizeof(buffer), "OK %ld\r\n", file_size);
            send_all(client_sock, buffer, strlen(buffer));

            int n;
            while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
                send(client_sock, buffer, n, 0);
            }

            fclose(fp);
            break;
        } else {
            send_all(client_sock, "ERROR file not found\r\n", strlen("ERROR file not found\r\n"));
        }
    }

    close(client_sock);
    exit(0);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (server_sock < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_sock, 5) < 0) {
        perror("listen");
        return 1;
    }

    printf("File server is running on port %d...\n", PORT);

    while (1) {
        client_len = sizeof(client_addr);

        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        printf("Client connected\n");

        pid_t pid = fork();

        if (pid == 0) {
            close(server_sock);
            handle_client(client_sock);
        } else if (pid > 0) {
            close(client_sock);
        } else {
            perror("fork");
            close(client_sock);
        }
    }

    close(server_sock);
    return 0;
}
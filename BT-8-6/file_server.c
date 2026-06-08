#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define PORT 8081
#define BUF_SIZE 8192

void send_404(int client) {
    const char *body =
        "<html>"
        "<head><meta charset='utf-8'><title>404</title></head>"
        "<body><h2>404 Not Found</h2></body>"
        "</html>";

    char response[BUF_SIZE];

    snprintf(response, sizeof(response),
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %ld\r\n"
        "\r\n"
        "%s",
        strlen(body), body);

    send(client, response, strlen(response), 0);
}

void send_header(int client, const char *content_type, long content_length) {
    char header[1024];

    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "\r\n",
        content_type, content_length);

    send(client, header, strlen(header), 0);
}

void url_decode(char *src) {
    char temp[2048];
    int i = 0, j = 0;

    while (src[i] != '\0' && j < 2047) {
        if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            char hex[3];
            hex[0] = src[i + 1];
            hex[1] = src[i + 2];
            hex[2] = '\0';

            temp[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } 
        else if (src[i] == '+') {
            temp[j++] = ' ';
            i++;
        } 
        else {
            temp[j++] = src[i++];
        }
    }

    temp[j] = '\0';
    strcpy(src, temp);
}

const char *get_content_type(const char *path) {
    const char *ext = strrchr(path, '.');

    if (ext == NULL) {
        return "application/octet-stream";
    }

    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".txt") == 0) return "text/plain; charset=utf-8";
    if (strcmp(ext, ".c") == 0) return "text/plain; charset=utf-8";
    if (strcmp(ext, ".cpp") == 0) return "text/plain; charset=utf-8";
    if (strcmp(ext, ".h") == 0) return "text/plain; charset=utf-8";

    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
    if (strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".bmp") == 0) return "image/bmp";

    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";

    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".webm") == 0) return "video/webm";

    return "application/octet-stream";
}

void send_file(int client, const char *path) {
    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        send_404(client);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    send_header(client, get_content_type(path), file_size);

    char buffer[BUF_SIZE];
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(client, buffer, n, 0);
    }

    fclose(fp);
}

void send_directory(int client, const char *path, const char *url_path) {
    DIR *dir = opendir(path);

    if (dir == NULL) {
        send_404(client);
        return;
    }

    char body[BUF_SIZE * 4];

    snprintf(body, sizeof(body),
        "<html>"
        "<head>"
        "<meta charset='utf-8'>"
        "<title>HTTP File Server</title>"
        "</head>"
        "<body>"
        "<h2>Directory: %s</h2>"
        "<ul>",
        url_path);

    if (strcmp(url_path, "/") != 0) {
        strncat(body,
            "<li><b><a href='..'>../</a></b></li>",
            sizeof(body) - strlen(body) - 1);
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0) continue;
        if (strcmp(entry->d_name, "..") == 0) continue;

        char full_path[2048];
        char link_path[2048];
        char line[4096];

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (strcmp(url_path, "/") == 0) {
            snprintf(link_path, sizeof(link_path), "/%s", entry->d_name);
        } else {
            snprintf(link_path, sizeof(link_path), "%s/%s", url_path, entry->d_name);
        }

        struct stat st;

        if (stat(full_path, &st) < 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            snprintf(line, sizeof(line),
                "<li><b><a href='%s'>%s/</a></b></li>",
                link_path, entry->d_name);
        } else {
            snprintf(line, sizeof(line),
                "<li><i><a href='%s'>%s</a></i></li>",
                link_path, entry->d_name);
        }

        strncat(body, line, sizeof(body) - strlen(body) - 1);
    }

    closedir(dir);

    strncat(body, "</ul></body></html>", sizeof(body) - strlen(body) - 1);

    char response[BUF_SIZE * 5];

    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %ld\r\n"
        "\r\n"
        "%s",
        strlen(body), body);

    send(client, response, strlen(response), 0);
}

void handle_request(int client, char *request) {
    char method[16];
    char url[2048];

    sscanf(request, "%15s %2047s", method, url);

    if (strcmp(method, "GET") != 0) {
        send_404(client);
        return;
    }

    url_decode(url);

    char path[4096];

    if (strcmp(url, "/") == 0) {
        snprintf(path, sizeof(path), ".");
    } else {
        snprintf(path, sizeof(path), ".%s", url);
    }

    struct stat st;

    if (stat(path, &st) < 0) {
        send_404(client);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        send_directory(client, path, url);
    } else {
        send_file(client, path);
    }
}

int main() {
    int server_fd;
    int client;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("File server running at http://localhost:%d\n", PORT);

    while (1) {
        client = accept(server_fd, NULL, NULL);

        if (client < 0) {
            perror("accept");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));

        recv(client, buffer, sizeof(buffer) - 1, 0);

        handle_request(client, buffer);

        close(client);

    }

    close(server_fd);

    return 0;
}
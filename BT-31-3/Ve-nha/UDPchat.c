#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s port_s ip_d port_d\n", argv[0]);
        return 1;
    }

    int port_s = atoi(argv[1]);
    char *ip_d = argv[2];
    int port_d = atoi(argv[3]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Thiết lập non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Bind cổng nhận
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port_s);

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return 1;
    }

    // Địa chỉ đích
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_d);
    inet_pton(AF_INET, ip_d, &dest_addr.sin_addr);

    printf("UDP Chat started. Type your messages:\n");
    printf("(Press Ctrl+C to exit)\n");

    fd_set read_fds;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        int max_fd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        // select với timeout 1 giây
        struct timeval tv = {1, 0};
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (activity < 0) {
            perror("Select error");
            break;
        }

        // Nhận tin nhắn
        if (FD_ISSET(sock, &read_fds)) {
            int n = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                            (struct sockaddr *)&sender_addr, &addr_len);
            if (n > 0) {
                buffer[n] = '\0';
                printf("\n[Received]: %s\n", buffer);
                printf("You: ");
                fflush(stdout);
            }
        }

        // Gửi tin nhắn từ stdin
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            fgets(buffer, BUFFER_SIZE, stdin);
            buffer[strcspn(buffer, "\n")] = 0; // Xóa newline

            if (strlen(buffer) > 0) {
                sendto(sock, buffer, strlen(buffer), 0,
                      (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            }
            printf("You: ");
            fflush(stdout);
        }
    }

    close(sock);
    return 0;
}
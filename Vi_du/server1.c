#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>

int main(){
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    int opt = 1;

    // thiet lap giai phong cong ngay khi server tat
    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("setsockopt() failed");
        exit(EXIT_FAILURE);
    }

    int ret = bind(listener, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0){
        perror("bind() failed");
        exit(1);
    }
    ret = listen(listener, 5);
    if(ret < 0){
        perror("listen() failed");
        exit(1);
    }

    printf("Waiting for client...\n");  

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    int client = accept(listener, (struct sockaddr*)&client_addr, &client_addr_len);
    if(client < 0){
        perror("accept() failed");
        exit(1);    
    }

    printf("Client connected: %d\n", client);
    printf("Ip: %s\n", inet_ntoa(client_addr.sin_addr));
    printf("Port: %d\n", ntohs(client_addr.sin_port));

    char msg[] = "Hello, client";
    send(client, msg, strlen(msg), 0);

    char buf[256];
    while(1){
        int n = recv(client, buf, sizeof(buf) - 1, 0);
        printf("%d bytes received\n", n);

        if(n <= 0){
            printf("Client disconnected\n");
            break;
        }
        buf[n] = '\0';
        printf("Received from client: %s\n", buf);

    }
    close(client);
    close(listener);

    return 0;

}
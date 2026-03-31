#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>

int main(){
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(9000);
    int ret = connect(client, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0){
        perror("connect( failed");
        return -1;  
    }

    char msg[256];
    while(1){
        printf("Enter string: ");
        fgets(msg, sizeof(msg), stdin);
        if(strncmp(msg, "exit", 4) == 0){
            break;
        }
        send(client, msg, strlen(msg), 0);
    }
    
    
    close(client);
    return 0;
}
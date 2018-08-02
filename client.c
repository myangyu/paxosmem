#include<stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    struct sockaddr_in sa;
    int port = atoi(argv[1]);
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (-1 == SocketFD) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
    memset(&sa, 0, sizeof sa);

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    printf("%d \n", port);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);

    if (-1 == connect(SocketFD, (struct sockaddr*)&sa, sizeof sa)) {
        perror("connect failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }
    char buffer[512];
    char input[512];
    char send_info[515];
    int totalRead = 0;
    for (;;) {
        int readSize = 0;
        printf(">");
        gets(input);
        sprintf(send_info, "-1,%s", input);
        printf("%s \n", send_info);
        int length = strlen(send_info);
        send(SocketFD, send_info, length, 0);
        readSize = recv(SocketFD, buffer, 512, 0);
        if(readSize > 0){
            buffer[readSize] = 0x00;
        }
        printf(">%s \n", buffer);
    }
    buffer[totalRead] = 0;

    (void)shutdown(SocketFD, SHUT_RDWR);
    close(SocketFD);
    return EXIT_SUCCESS;
}


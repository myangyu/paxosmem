/*************************************************************************
    > File Name: client.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2017年12月15日 17时37分29秒 CST
 ************************************************************************/

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
//    char hello[] = "set hello world";
    struct sockaddr_in sa;
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (-1 == SocketFD) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
    memset(&sa, 0, sizeof sa);

    sa.sin_family = AF_INET;
    sa.sin_port = htons(12000);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);

    if (-1 == connect(SocketFD, (struct sockaddr*)&sa, sizeof sa)) {
        perror("connect failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }
    char buffer[512];
    char input[512];
    int totalRead = 0;
    for (;;) {
        int readSize = 0;
//        send(SocketFD, hello, 15, 0);
//        write(SocketFD, hello, 15);
//        write(SocketFD, "set aaaaa ccssc", 15);
//        if (readSize == 0) {
//            break;
//        }
//        else if (readSize == -1) {
//            perror("read failed");
//            close(SocketFD);
//            exit(EXIT_FAILURE);
//        }
//        totalRead += readSize;
        printf(">");
        gets(input);
        int length = strlen(input);
        send(SocketFD, input, length, 0);
        readSize = recv(SocketFD, buffer, 512, 0);
        if(readSize > 0){
            buffer[readSize] = 0x00;
        }
        printf(">%s \n", buffer);
    }
    buffer[totalRead] = 0;
    printf("get from server:%s\n", buffer);
    
    (void)shutdown(SocketFD, SHUT_RDWR);
    close(SocketFD);
    return EXIT_SUCCESS;
}


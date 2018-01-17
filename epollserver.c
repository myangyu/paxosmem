#include  <unistd.h>
#include  <sys/types.h>       /* basic system data types */
#include  <sys/socket.h>      /* basic socket definitions */
#include  <netinet/in.h>      /* sockaddr_in{} and other Internet defns */
#include  <arpa/inet.h>       /* inet(3) functions */
#include <sys/epoll.h> /* epoll function */
#include <fcntl.h>     /* nonblocking */
#include <sys/resource.h> /*setrlimit */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "dispatcher.h"
#include "config.h"

#define STRING_MAX_SIZE  1000
#define STRING_MOD  STRING_MAX_SIZE


#define MAXEPOLLSIZE 10000
#define MAXLINE 10240

//提议初始id
int propose_id = 1;
//服务状态
char *status = "sync";
//备份节点已连接个数
int connected_num = 0;
//备份节点连接fd数组
int backups_fd[backups_num-1] = {0};
//fd数组的index
int fd_index = 0;
//已经同步完成的节点个数
int backuped_num = 0;
//已经同步的端口list
int connected_ports[5] = {0};
int connected_ports_index = 0;

int handle(int connfd, struct hash_table *ht, struct funcs *f);
int in_list_int(int num, int list[], int length);
int setnonblocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
		return -1;
	}
	return 0;
}

//struct fmap{
//    int fd;
//    void (*fun)(int connfd);
//}fmap;


int main(int argc, char **argv)
{

    char *BACKUPS_PORTS = "12000 12001 12002 12003 12004";
	int  servPort = atoi(argv[1]);
	int listenq = 1024;

	int listenfd, connfd, kdpfd, nfds, n, nread, curfds, acceptCount = 0;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t socklen = sizeof(struct sockaddr_in);
	struct epoll_event ev;
	struct epoll_event events[MAXEPOLLSIZE];
	struct rlimit rt;
	char buf[MAXLINE];

    char *backups_ports = malloc(sizeof(char) * 30);
    sprintf(backups_ports, "%s", BACKUPS_PORTS);
    //获取备份节点的端口
    char **ports = malloc(sizeof(char) * 8 * 5);
    parse_io_stream(backups_ports, ports);
//    printf("port \n%s\n%s\n%s\n%s\n",ports[0], ports[1],ports[2],ports[3]);

    /* 初始化 hash_table funcs */
    struct hash_table *ht;
    ht = malloc(sizeof(hash_table));
    long l = STRING_MAX_SIZE;
    ht->length = STRING_MAX_SIZE;
    ht->used = 0;
    struct string *nds = malloc(sizeof(string) * l);
    for(int i=0; i<l; i++){
        nds[i].key = NULL;
    }
    ht->node = nds;
    struct funcs *f = malloc(sizeof(funcs));
    char **params = malloc(sizeof(char) * 8 * 4);
    f->params = params;

	/* 设置每个进程允许打开的最大文件数 */
	rt.rlim_max = rt.rlim_cur = MAXEPOLLSIZE;
	if (setrlimit(RLIMIT_NOFILE, &rt) == -1) 
	{
		perror("setrlimit error");
		return -1;
	}


    //监听 address and port bind and listen
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(servPort);

	listenfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (listenfd == -1) {
		perror("can't create socket file");
		return -1;
	}
	int opt = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (setnonblocking(listenfd) < 0) {
		perror("setnonblock error");
	}
    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr)) == -1)
	{
		perror("bind error");
		return -1;
	}
	if (listen(listenfd, listenq) == -1)
	{
		perror("listen error");
		return -1;
	}
    kdpfd = epoll_create(MAXEPOLLSIZE);
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = listenfd;
	/* 创建 epoll 句柄，把监听 socket 加入到 epoll 集合里 */
	if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, listenfd, &ev) < 0)
	{
		fprintf(stderr, "epoll set insertion error: fd=%d\n", listenfd);
		return -1;
	}

    // 处理客户端请求事件
	curfds = 100;
	printf("epollserver startup,port %d, max connection is %d, backlog is %d\n", servPort, MAXEPOLLSIZE, listenq);
	for (;;) {
		/* 等待有事件发生 */
		nfds = epoll_wait(kdpfd, events, curfds, 1);
		if (nfds == -1)
		{
			perror("epoll_wait");
			continue;
		}

		//连接备份节点，将连接成功的fd加入epoll
        if(str_equal(status, "sync")){
            if(connected_num < backups_num-1){
                for(int i=0; i<backups_num; i++){
                    int syn_node_port = atoi(ports[i]);
                    if(servPort==syn_node_port || 1==in_list_int(syn_node_port, connected_ports, backups_num))
                        continue;
                    struct sockaddr_in *sa =  malloc(sizeof(struct sockaddr_in));
                    struct epoll_event syn_ev;
                    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (-1 == SocketFD) {
                        perror("cannot create socket");
                        free(sa);
                        continue;
                    }

                    memset(sa, 0, sizeof *sa);
                    sa->sin_family = AF_INET;
                    sa->sin_port = htons(syn_node_port);
                    inet_pton(AF_INET, "127.0.0.1", &sa->sin_addr);
                    if (-1 == connect(SocketFD, (struct sockaddr*)sa, sizeof *sa)) {
                        perror("connect failed aa");
                        close(SocketFD);
                        sleep(2);
                        continue;
                    }
                    backups_fd[fd_index] = SocketFD;
                    fd_index += 1;
                    syn_ev.events = EPOLLIN | EPOLLET;
                    syn_ev.data.fd = SocketFD;
                    if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, SocketFD, &syn_ev) < 0)
                    {
                        fprintf(stderr, "sync socket '%d' to epoll failed: %s\n", SocketFD, strerror(errno));
                        return -1;
                    }
                    send(SocketFD, "connect", 7, 0);

                    connected_num += 1;
                    connected_ports[connected_ports_index] = syn_node_port;
                    connected_ports_index++;
                    sleep(1);
                    printf("connected %d socket, all connected %d\n", SocketFD, connected_num);
                }
            }else{
                for(int i=0; i<connected_num; i++){
                    int fd = backups_fd[i];
                    send(fd, "sync", 4, 0);
                    printf("fd %d send sync", fd);
                }
                sleep(1);
            }
        }
        printf("%d %d %d %d \n", connected_ports[0], connected_ports[1], connected_ports[2], connected_ports[3]);
        printf("envents counts %d and status %s\n", nfds, status);

		/* 处理所有事件 */
		for (n = 0; n < nfds; ++n)
		{
		    //备份节点的同步
		    if(1==str_equal(status, "sync")){
		        if(backuped_num < backups_num -1){
                    int connfd = events[n].data.fd;
                    if(1==in_list_int(connfd, backups_fd, 4)){
                        char *buf = malloc(sizeof(char) * 10);
                        printf("aaaa dd\n");
                        int flag = recv(connfd, buf, MAXLINE, 0);//读取客户端socket流
                        printf("aaaa %d", flag);
                        if(flag <= 0){
                            perror("recv error");
                            continue;
                        }
                        buf[flag]=0x00;
                        if(1==str_equal("sync", buf)){
                            backuped_num += 1;
                            printf("sync num %d", backuped_num);
                        }
                    }
                }else{
                    status = "grab";
                }
		    }
		    // 抢占leader
		    if(1==str_equal(status, "grab")){

		    }
		    // 处理用户请求
		    if(1==str_equal(status, "active")){
                if (events[n].data.fd == listenfd)
                {
                    connfd = accept(listenfd, (struct sockaddr *)&cliaddr,&socklen);
                    if (connfd < 0)
                    {
                        perror("accept error");
                        continue;
                    }

                    sprintf(buf, "accept form %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
                    printf("%d:%s", ++acceptCount, buf);

                    if (curfds >= MAXEPOLLSIZE) {
                        fprintf(stderr, "too many connection, more than %d\n", MAXEPOLLSIZE);
                        close(connfd);
                        continue;
                    }
                    if (setnonblocking(connfd) < 0) {
                        perror("setnonblocking error");
                    }
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = connfd;
                    if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, connfd, &ev) < 0)
                    {
                        fprintf(stderr, "add socket '%d' to epoll failed: %s\n", connfd, strerror(errno));
                        return -1;
                    }
                    curfds++;
                    continue;
                }else if (handle(events[n].data.fd, ht, f) < 0) {
                    epoll_ctl(kdpfd, EPOLL_CTL_DEL, events[n].data.fd,&ev);
                    curfds--;
                }
            }



		}
	}
	close(listenfd);
	return 0;
}


int handle(int connfd, struct hash_table *ht, struct funcs *f) {
	char *buf = malloc(sizeof(char) * MAXLINE);
	int flag = read(connfd, buf, MAXLINE);//读取客户端socket流
	if(flag <= 0){
	    return 0;
	}
	buf[flag]=0x00;
    parse(buf, f);
    f->do_data(connfd, f->params, f->params_count, ht);
	return 0;
}


int in_list_int(int num, int list[], int length){
    for(int i=0; i< length; i++){
        if(num==list[i])
            return 1;
    }
    return 0;
}

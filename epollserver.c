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
int propose_id = 1;
int handle(int connfd, struct hash_table *ht, struct funcs *f, int nodes_fd[]);
int setnonblocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
		return -1;
	}
	return 0;
}


struct fmap{
    int fd;
    void (*fun)(int connfd);
}fmap;


int main(int argc, char **argv)
{
	int  servPort = 6888;
	int listenq = 1024;

	int listenfd, connfd, syn_kdpfd, kdpfd, nfds, n, nread, curfds, sync_listenfd, acceptCount = 0;
	struct sockaddr_in servaddr, cliaddr, sync_addr;
	socklen_t socklen = sizeof(struct sockaddr_in);
	struct epoll_event ev, sync_ev;
	struct epoll_event events[MAXEPOLLSIZE], syn_events[MAXEPOLLSIZE];
	struct rlimit rt;
	char buf[MAXLINE];
	int nodes_fd[5] = {0};
	int node_fd_index = 0;

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



    //同步 address and port and bind and listen
    bzero(&sync_addr, sizeof(sync_addr));
	sync_addr.sin_family = AF_INET;
	sync_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sync_addr.sin_port = htons(SYN_PORT);

	sync_listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sync_listenfd == -1) {
		perror("can't create sync socket file");
		return -1;
	}

	int sync_opt = 2;
    setsockopt(sync_listenfd, SOL_SOCKET, SO_REUSEADDR, &sync_opt, sizeof(sync_opt));

	if (setnonblocking(sync_listenfd) < 0) {
		perror("sync_listenfd setnonblock error");
	}

	if (bind(sync_listenfd, (struct sockaddr *) &sync_addr, sizeof(struct sockaddr)) == -1)
	{
		perror("sync_listenfd bind error");
		return -1;
	}

	if (listen(sync_listenfd, listenq) == -1)
	{
		perror("sync_listenfd listen error");
		return -1;
	}

	sync_ev.events = EPOLLIN | EPOLLET;
	sync_ev.data.fd = sync_listenfd;
	if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, sync_listenfd, &sync_ev) < 0)
	{
		fprintf(stderr, "epoll set sync_listenfd insertion error: fd=%d\n", sync_listenfd);
		return -1;
	}
    //创建 paxos 一致性协调 epoll
    syn_kdpfd = epoll_create(MAXEPOLLSIZE);


    // 处理客户端请求事件
	curfds = 2;
	printf("epollserver startup,port %d, max connection is %d, backlog is %d\n", servPort, MAXEPOLLSIZE, listenq);
	for (;;) {
		/* 等待有事件发生 */
		nfds = epoll_wait(kdpfd, events, curfds, -1);
		if (nfds == -1)
		{
			perror("epoll_wait");
			continue;
		}
		/* 处理所有事件 */
		for (n = 0; n < nfds; ++n)
		{
		    //新客户端连接请求 处理
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
			}
			// paxos下 新的节点加入处理
			else if(events[n].data.fd == sync_listenfd){
			    connfd = accept(sync_listenfd, (struct sockaddr *)&cliaddr,&socklen);
			    char *buf = malloc(sizeof(char) * MAXLINE);
                int flag = read(connfd, buf, MAXLINE);//读取客户端socket流
                struct sockaddr_in syn_node_addr;
                int sync_node_listenfd = 0;
                if(flag <= 0){
                    return 0;
                }
                buf[flag]=0x00;
                char **real_params = malloc(sizeof(char) * 8 * 2);
                parse_io_stream(buf, real_params);

                //同步 address and port
                int syn_node_port = atoi(real_params[1]);
                bzero(&syn_node_addr, sizeof(syn_node_addr));
                syn_node_addr.sin_family = AF_INET;
                syn_node_addr.sin_addr.s_addr = htonl(INADDR_ANY);
                syn_node_addr.sin_port = htons(syn_node_port);

                sync_node_listenfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sync_node_listenfd == -1) {
                    perror("can't create sync socket file");
                    return -1;
                }

                int opt = 2;
                setsockopt(sync_node_listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                if (setnonblocking(sync_node_listenfd) < 0) {
                    perror("setnonblock error");
                }

                if (bind(sync_node_listenfd, (struct sockaddr *) &syn_node_addr, sizeof(struct sockaddr)) == -1)
                {
                    perror("bind error");
                    return -1;
                }
                if (listen(sync_node_listenfd, listenq) == -1)
                {
                    perror("listen error");
                    return -1;
                }

                struct epoll_event node_ev;
                node_ev.events = EPOLLIN | EPOLLET;
                node_ev.data.fd = sync_listenfd;
                if (epoll_ctl(syn_kdpfd, EPOLL_CTL_ADD, sync_node_listenfd, &node_ev) < 0)
                {
                    fprintf(stderr, "epoll set sync_listenfd insertion error: fd=%d\n", sync_node_listenfd);
                    return -1;
                }
                nodes_fd[node_fd_index] = sync_node_listenfd;
			}

			// 处理客户端请求
			if(MASTER == 1 && PAXOS==1){
			    int syn_nfds = 0;
			    int syn_fd[5] = {0};
			    char paxos_prepare[50];
			    char paxos_commit[MAXLINE + 50];
                sprintf(paxos_prepare, "paxos %d prepare", propose_id);
                int length = strlen(paxos_info);
                for(int i=0; i<5; i++){
                    if(nodes_fd[i] > 0){
                        send(nodes_fd[i], paxos_prepare, length, 0);
                    }
                }
                syn_nfds = epoll_wait(syn_kdpfd, syn_events, 100, 1000);
                if(syn_nfds < 3){
                    perror(" syn node failed expected 5 exactly %d", syn_nfds);
                    continue;
                }
                for(int i=0; i<syn_nfds; i++){
                    int temp_fd = syn_nfds[n].data.fd;
                    syn_fd[i] = temp_fd;
                }


			}
			else if (handle(events[n].data.fd, ht, f, nodes_fd) < 0) {
				epoll_ctl(kdpfd, EPOLL_CTL_DEL, events[n].data.fd,&ev);
				curfds--;
			}

		}
	}
	close(listenfd);
	return 0;
}


int handle(int connfd, struct hash_table *ht, struct funcs *f, int nodes_fd[]) {
	char *buf = malloc(sizeof(char) * MAXLINE);
	int flag = read(connfd, buf, MAXLINE);//读取客户端socket流
	if(flag <= 0){
	    return 0;
	}
	buf[flag]=0x00;
//	printf("recv %s \n", buf);
    parse(buf, f);
    f->do_data(connfd, f->params, f->params_count, ht);
//    int length = strlen(result);
//    send(connfd, result, length, 0);
//	write(connfd, result, length);//响应客户端
	return 0;
}

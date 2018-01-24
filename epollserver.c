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
int propose_step = 0;
//服务状态
char *status = "sync";
//备份节点已连接个数
int connected_num = 0;
//备份节点连接写fd数组
int backups_fd_w[backups_num-1] = {0};
//备份节点的读fd
int backups_fd_r[backups_num-1] = {0};
//fd写数组的index
int fd_index_w = 0;
//fd读数组的index
int fd_index_r = 0;

//已经同步完成的节点个数
int backuped_num = 0;
//已经同步的fd
int backuped_fd[backups_num-1] = {0};
//已经同步的端口list
int connected_ports[5] = {0};
int connected_ports_index = 0;

//start paxos
int start_paxos = 0;
//prepare counts
int prepare_c=0;
//commit counts
int commit_c=0;

//sucess_c
int sucess_c=0;
//client_fd
int client_fd = 0;
int LEADER = 0;
int handle(int connfd, struct hash_table *ht, struct funcs *f);
int in_list_int(int num, int list[], int length);
void connect_nodes(int kdpfd, int servPort, char **ports);
void recv_sync(int connfd);
int epoll_add(int kdpfd, int connfd, int *curfds, struct sockaddr_in *cliaddr);
void commit_step(int *propose_step, int *prepare_c, int *commit_c,
                    int *start_paxos, struct funcs *f, struct hash_table *ht);
void prepare_step(int connfd, int *start_paxos, struct funcs *f, struct hash_table *ht);
int setnonblocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
		return -1;
	}
	return 0;
}


int main(int argc, char **argv)
{

    char *BACKUPS_PORTS = "12000 12001 12002";
	int  servPort = atoi(argv[1]);
	int listenq = 1024;

	int listenfd, connfd, kdpfd, nfds, n, nread, curfds;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t socklen = sizeof(struct sockaddr_in);
	struct epoll_event ev;
	struct epoll_event events[MAXEPOLLSIZE];
	struct rlimit rt;

    char *backups_ports = malloc(sizeof(char) * 30);
    sprintf(backups_ports, "%s", BACKUPS_PORTS);
    //获取备份节点的端口
    char **ports = malloc(sizeof(char) * 8 * 5);
    parse_io_stream(backups_ports, ports);

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
	ev.events = EPOLLIN;
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
		nfds = epoll_wait(kdpfd, events, curfds, 3000);
		if (nfds == -1)
		{
			perror("epoll_wait");
			continue;
		}
		//连接备份节点，将连接成功的fd加入epoll
        if(str_equal(status, "sync")){
            connect_nodes(kdpfd, servPort, ports);
        }
		/* 处理所有事件 */
		for (n = 0; n < nfds; n++)
		{
            connfd = events[n].data.fd;
		    //监听备份节点
		    if(1==str_equal(status, "sync")){
		        if (events[n].data.fd == listenfd){
                    connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &socklen);
                    int ret = epoll_add(kdpfd, connfd, &curfds, &cliaddr);
                    if(1==ret){
                        curfds++;
                        backups_fd_r[fd_index_r] = connfd;
                        fd_index_r++;
                    }
                    continue;
		        }
//		        处理备份节点同步
		        recv_sync(connfd);
		    }
//		     抢占leader todo paxos选举leader
		    if(1==str_equal(status, "grab")){
                if(servPort == 12000){
                    LEADER = 1;
                    printf("i am the leader \n");
                }
                status = "active";
                printf("status is active");
		    }

		    // 正常状态，处理用户请求
		    if(1==str_equal(status, "active")){
		        connfd = events[n].data.fd;

//		        client 建立链接
                if (events[n].data.fd == listenfd)
                {
                    connfd = accept(listenfd, (struct sockaddr *)&cliaddr,&socklen);
                    int ret = epoll_add(kdpfd, connfd, &curfds, &cliaddr);
                    if(ret==1)
                        curfds++;
                    continue;
                }
//              leader节点接受外部请求， 进行commit阶段 todo 非leader节点 负载读操作
                if(1==in_list_int(connfd, backups_fd_w, backups_num-1)){
                    if(LEADER == 1){
                        printf("commit step \n");
                        commit_step(&propose_step, &prepare_c, &commit_c, &start_paxos, f, ht);
                    }
                }
                //leader节点接受外部请求， 进行prepare阶段提议
                else if(0==in_list_int(connfd, backups_fd_r, backups_num-1)){
                    prepare_step(connfd, &start_paxos, f, ht);
                }
                // 非leader节点执行二阶段提交
                else if(handle(events[n].data.fd, ht, f) < 0) {
                        if(0==in_list_int(events[n].data.fd, backups_fd_w, backups_num-1)
                           && 0==in_list_int(events[n].data.fd, backups_fd_r, backups_num-1)){
                            epoll_ctl(kdpfd, EPOLL_CTL_DEL, events[n].data.fd,&ev);
                            curfds--;
                    }
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


void connect_nodes(int kdpfd, int servPort, char **ports){

    if(connected_num < backups_num-1){
        for(int i=0; i< backups_num; i++){
            int syn_node_port = atoi(ports[i]);
            int r = in_list_int(syn_node_port, connected_ports, backups_num);
            if(syn_node_port<=0 || servPort==syn_node_port || 1==in_list_int(syn_node_port, connected_ports, backups_num))
                continue;

            struct sockaddr_in sa;
            struct epoll_event ev;
            int SocketFD;

            SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (-1 == SocketFD) {
                perror("cannot create socket");
                continue;
            }
            memset(&sa, 0, sizeof sa);
            sa.sin_family = AF_INET;
            sa.sin_port = htons(syn_node_port);
            inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
            if (-1 == connect(SocketFD, (struct sockaddr*)&sa, sizeof sa)) {
                perror("connect failed");
                close(SocketFD);
                sleep(1);
                continue;
            }

            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = SocketFD;
            if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, SocketFD, &ev) < 0)
                continue;
            backups_fd_w[fd_index_w] = SocketFD;
            send(SocketFD, "sync", 4, 0);
            fd_index_w += 1;
            connected_num += 1;
            connected_ports[connected_ports_index] = syn_node_port;
            connected_ports_index += 1;
            printf("connected %d socket port is %d, all connected %d\n", SocketFD, syn_node_port, connected_num);
        }
        sleep(2);
    }
}

void recv_sync(int connfd){

    if(1==in_list_int(connfd, backups_fd_r, backups_num-1) && 0==in_list_int(connfd, backuped_fd, backups_num-1)){
        char buf[5];
        int flag = recv(connfd, buf, 4, 0);//读取客户端socket流
        if(flag <= 0){
            perror("recv error");
        }else{
            buf[flag]=0x00;
            if(1==str_equal("sync", buf)){
                backuped_fd[backuped_num] = connfd;
                backuped_num += 1;
            }
            printf("recv info %s num %d\n", buf, backuped_num);
        }
    }
    if(backuped_num == backups_num-1){

        status = "grab";
        printf("status %s \n", status);
    }
}

int epoll_add(int kdpfd, int connfd, int *curfds, struct sockaddr_in *cliaddr){
    if (connfd < 0)
        {
            perror("accept error");
            return -1;
        }
        char buf_client[MAXLINE];
        sprintf(buf_client, "accept form %s:%d\n", inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
        printf("%s", buf_client);

        if (*curfds >= MAXEPOLLSIZE) {
            fprintf(stderr, "too many connection, more than %d\n", MAXEPOLLSIZE);
            close(connfd);
            return -1;
        }
        if (setnonblocking(connfd) < 0) {
            perror("setnonblocking error");
        }
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = connfd;
        if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, connfd, &ev) < 0)
        {
            fprintf(stderr, "add socket '%d' to epoll failed: %s\n", connfd, strerror(errno));
            return -1;
        }
        return 1;
}

void commit_step(int *propose_step, int *prepare_c, int *commit_c,
                    int *start_paxos, struct funcs *f, struct hash_table *ht){
    if(*propose_step==0 && *start_paxos==1){
        *prepare_c += 1;
        if(*prepare_c >= backups_num -1){
            char propose[MAXLINE + 30];
            int pc = f->params_count;
            sprintf(propose, "paxos %d commit ", propose_id);
            for(int i=0; i<pc; i++){
                sprintf(propose, "%s%s", propose, f->params[i]);
                if(i < pc-1)
                    sprintf(propose, "%s%s", propose, "_");
            }
            int length = strlen(propose);
            for(int i=0; i<connected_num; i++){
                int fd = backups_fd_w[i];
                int ret = send(fd, propose, length, 0);
            }
            *propose_step=1;
        }
    }else if(1==*propose_step){
        *commit_c += 1;
        if(*commit_c >= backups_num-1){
            f->do_data(client_fd, f->params, f->params_count, ht);
            *prepare_c = *commit_c = *propose_step = *start_paxos = 0;
            propose_id+=1;
        }
    }
}

void prepare_step(int connfd, int *start_paxos, struct funcs *f, struct hash_table *ht){
        client_fd = connfd;
        char propose[MAXLINE + 30];
        char *buf = malloc(sizeof(char) * MAXLINE);
        int flag = read(connfd, buf, MAXLINE);//读取客户端socket流
        if(flag <= 0){
            perror("accept error");
        }else{
            buf[flag]=0x00;
            parse(buf, f);
            int pc = f->params_count;
            sprintf(propose, "paxos %d prepare ", propose_id);
            printf("aaa %s \n", propose);
            for(int i=0; i<pc; i++){
                sprintf(propose, "%s%s", propose, f->params[i]);
                if(i < pc-1)
                    sprintf(propose, "%s%s", propose, "_");
            }
            if(1==str_equal("get", f->params[0])){
                f->do_data(connfd, f->params, f->params_count, ht);
            }else{
                int length = strlen(propose);
                for(int i=0; i<connected_num; i++){
                    int fd = backups_fd_w[i];
                    int ret = send(fd, propose, length, 0);
                    printf("aaa %s \n", propose);
                }
                *start_paxos = *start_paxos + 1;
            }
        }
}
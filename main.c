#include<stdio.h>
#include<string.h>
#include<errno.h>
#include <stdlib.h>
#include<unistd.h>
#include<event.h>
#include  <sys/socket.h>      /* basic socket definitions */
#include <arpa/inet.h>
#include "lib/str.h"
#include <unistd.h>

struct Nodes{
    int deny_p;
    int deny_c;
    int fd;
    char *propose;
    int propose_id;
    int id;
    char *role;
} Nodes;

void socket_read_cb(int fd, short events, void*arg);

int tcp_server_init(int port, int listen_num);
int parse_io_stream(char info[], char **params, char *separator);
void accept_fd(evutil_socket_t listener, short event, void *arg);
void connect_nodes(int fd, short events, void*arg);
void graber_leader(int fd, short events, void*arg);
struct Paxosinstance* new_paxos_instance(int num);
void handle_nodes_fd(int fd);
int get_propose_id(void);
int deal_commit(void);
void deal_prepare(void);
void reset_nodes(struct Nodes *nodes);
void send_info(int fd, char *param_info);

void deal_prepare_null_ack(int fd, int proposer_id, char *propose);
void deal_prepare_deny_p_ack(int fd, int proposer_id);
void deal_prepare_propose_ack(int fd, int proposer_id, int propose_id, char *propose);
void deal_commit_accepted_ack(int proposer_id);
void deal_commit_deny_c_ack(int proposer_id);
int init_node(int fd, int proposer_id);
void prepare_agin(void);
void broad_to_nodes(void);
void deal_nodes_role(char *learning_propose);
struct hash_table* init_hash_table(long l);
//用于connect事件的，包含ev和connect链接的端口
struct connect_event{
    int port;
    struct event *ev;
} connect_event;

struct Paxosinstance{
    int counts_nodes;
    struct Nodes *nodes;
    int *prepare_ack_ids;
    int *commit_ack;
    char *status;
    int uninit_node_index;
}Paxosinstance;


// 存储node节点状态的hashtable
struct hash_table *nodeHT;
// 节点的个数
int count_nodes = 3;
// 已经链接的节点个数
int count_connected_nodes=1;

char **ports ;
//承诺的propose_id 和propose
int promise_propose_id = -1;
char *promise_propose;
int max_recive_propose_id=-1;
//本节点的初始propose_id
int propose_id;
int propose_id_interval = 3;
char *propose;
// 被多数派接受的议题
char *success_propose;
int servPort;
// 是否leader节点
int leader = -1;
struct Paxosinstance *instance;


int main(int argc, char**argv) {
    char *backups_ports = malloc(sizeof(char) * 30);
    sprintf(backups_ports, "%s %s", argv[3], argv[4]);
    instance = malloc(sizeof(struct Paxosinstance));
    new_paxos_instance(3);
    ports = malloc(sizeof(char) * 8 * 5);
    nodeHT = init_hash_table(3);
    //获取备份节点的端口
    parse_io_stream(backups_ports, ports, " ");
    
    servPort = atoi(argv[1]);
    propose_id = atoi(argv[2]);
    
    promise_propose = malloc(sizeof(char) * 24);
    propose = malloc(sizeof(char) * 24);
    success_propose = malloc(sizeof(char) * 24);
    sprintf(propose, "%d_leader", servPort);
    sprintf(promise_propose, "NULL");
    int listener = tcp_server_init(servPort, 10);
    if (-1==listener) {
        perror("tcp_server_init error\n");
        return -1;
    }
    
    struct event_base *Base;

    Base = event_init();

    struct event *ev = malloc(sizeof(struct event));
    event_set(ev, listener, EV_READ | EV_PERSIST, accept_fd, ev);
    event_add(ev, NULL);
    //注册事件
    for(int i=0; i<=1; i++){
        struct event *timeout = malloc(sizeof(struct event));
        struct timeval *tv = malloc(sizeof(struct timeval));
        struct connect_event *ce = malloc(sizeof(connect_event));
        ce->port = atoi(ports[i]);
        ce->ev = timeout;
        tv->tv_usec = 100;
        evtimer_set(timeout, connect_nodes, ce);
        evutil_timerclear(tv);
        event_add(timeout, tv);
    }
    event_dispatch();
    return 0;
}


void accept_fd(evutil_socket_t listener, short event, void *arg){
    evutil_socket_t fd;
    struct sockaddr_in sin;
    socklen_t slen;
    fd = accept(listener, (struct sockaddr *)&sin, &slen);
    if (fd < 0) {
        perror("accept");
        return;
    }
    if (fd > FD_SETSIZE) {
        perror("fd > FD_SETSIZE\n");
        return;
    }
    
    struct event *ev = malloc(sizeof(struct event));
    event_set(ev, fd, EV_READ | EV_PERSIST, socket_read_cb, ev);
    event_add(ev, NULL);
}


//处理读事件包含客户端的和节点的同步事件，paxos通讯等
void socket_read_cb(int fd, short events, void*arg) {
    char msg[4096];
    struct event*ev = (struct event*) arg;
    int len = (int) read(fd, msg, sizeof(msg) - 1);
    if (len <= 0) {
        printf("some error happen when read %d\n", len);
        event_free(ev);
        close(fd);
        return;
    }
    msg[len] = '\0';
    printf("recv the clientfd (%d) msg: %s %d \n",fd, msg, len);
    char **tempmsg = malloc(sizeof(char) * 30 * 4);
    parse_io_stream(msg, tempmsg, ",");
    int recieve_proposer_id = atoi(tempmsg[0]);
    int msg_count = parse_io_stream(tempmsg[1], tempmsg, "&");
    for(int i=0; i<msg_count; i++){
        char **info = malloc(sizeof(char) * 30 * 4);
        parse_io_stream(tempmsg[i], info, " ");
        //处理prepare事件  accepotor role
        if(str_equal(info[0], "prepare") == 1){
            printf("\033[31m acceptor start........ \033[0m \n");
            printf("prepare event recv info %s\n", info[1]);
            int recv_propose_id = atoi(info[1]);
            //从没有接到过prepare
            if(max_recive_propose_id < 0){
                char reply_msg[32] = "NULL";
                send_info(fd, reply_msg);
                if(str_equal(promise_propose, "NULL") == 1){
                    promise_propose_id = atoi(info[1]);
                }
                max_recive_propose_id = atoi(info[1]);
                printf("return %s \n", reply_msg);
            }
            //已经收到的propose_id大于请求propose_id 拒绝
            else if(max_recive_propose_id > recv_propose_id){
                char reply_msg[32] = "deny_p";
                send_info(fd, reply_msg);
                printf("return %s \n", reply_msg);
            }
            //已经收到的propose_id 小于请求的propose_id 则告知已收到的propose
            else if(max_recive_propose_id <= recv_propose_id){
                char reply_msg[32];
                sprintf(reply_msg, "propose %d %s", promise_propose_id, promise_propose);
                send_info(fd, reply_msg);
                if(str_equal(promise_propose, "NULL") == 1){
                    promise_propose_id = atoi(info[1]);
                }
                max_recive_propose_id = atoi(info[1]);
                printf("return %s \n", reply_msg);
            }
            printf("prepare info end and promise_propose_id %d, propose %s \n", max_recive_propose_id, promise_propose);
            printf("\033[31m acceptor end........ \033[0m \n");
        }
        //处理 commit 事件  accepotor role
        else if(str_equal(info[0], "commit") == 1){
            printf("\033[31m acceptor start........ \033[0m \n");
            printf("commit event recv info %s %s %s\n", info[0], info[1], info[2]);
            int recv_propose_id = atoi(info[1]);
            if(recv_propose_id < max_recive_propose_id){
                char reply_msg[24] = "deny_c";
                send_info(fd, reply_msg);
                printf("return %s \n", reply_msg);
            }else{
                char reply_msg[24] = "accepted";
                promise_propose_id = atoi(info[1]);
                max_recive_propose_id = atoi(info[1]);
                sprintf(promise_propose, "%s", info[2]);
                printf("promise_propose %s \n", promise_propose);
                send_info(fd, reply_msg);
                printf("return %s \n", reply_msg);
            }
            printf("commit event end  promise_propose_id %d  propose %s \n", promise_propose_id, promise_propose);
            printf("\033[31m acceptor end........ \033[0m \n");
        }
        // proposer role
        if(str_equal(info[0], "NULL") == 1){
            char *propose_value = malloc(sizeof(char) * 32);
            sprintf(propose_value, "%s", info[0]);
            deal_prepare_null_ack(fd, recieve_proposer_id, propose_value);
        }
        if(str_equal(info[0], "deny_p") == 1){
            deal_prepare_deny_p_ack(fd, recieve_proposer_id);
        }
        
        if(str_equal(info[0], "propose") == 1){
            char *propose_value = malloc(sizeof(char) * 32);
            int recv_propose_id = atoi(info[1]);
            sprintf(propose_value, "%s", info[2]);
            deal_prepare_propose_ack(fd, recieve_proposer_id, recv_propose_id, propose_value);
        }
        
        if(str_equal(info[0], "deny_c") == 1){
            deal_commit_deny_c_ack(recieve_proposer_id);
        }
        
        if(str_equal(info[0], "accepted") == 1){
            deal_commit_accepted_ack(recieve_proposer_id);
        }
    
        // learning去学习
        if(str_equal(info[0], "learning") == 1){
            deal_nodes_role(info[1]);
        }
        free(info);
    }
    free(tempmsg);
}


// 初始化一个server fd
int tcp_server_init(int port, int listen_num) {
    int errno_save;
    evutil_socket_t listener;
    
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        return -1;
    }
    //允许多次绑定同一个地址，要用在socket和bind之间
    evutil_make_listen_socket_reuseable(listener);
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);
    if (bind(listener, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
        errno_save = errno;
        evutil_closesocket(listener);
        errno = errno_save;
        return -1;
    }
    if (listen(listener, listen_num) < 0) {
        errno_save = errno;
        evutil_closesocket(listener);
        errno = errno_save;
        return -1;
    }
    evutil_make_socket_nonblocking(listener);
    return listener;
}


int parse_io_stream(char info[], char **params, char *separator){
    char* token = strtok(info, separator);
    int flag = 0;
    while( token != NULL )
    {
        params[flag] = token;
        token = strtok(NULL, separator);
        flag++;
    }
    return flag;
}


//定时任务用于链接其他节点，链接成功，事件注销并注册grab_leader回调事件
void connect_nodes(int fd, short events, void*arg){
    struct sockaddr_in sa;
    struct connect_event *ce = arg;
    int SocketFD;
    
    SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (-1 == SocketFD) {
        perror("cannot create socket");
    }
    
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(ce->port);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    if (-1 == connect(SocketFD, (struct sockaddr*)&sa, sizeof sa)) {
        close(SocketFD);
        struct timeval tv;
        evutil_timerclear(&tv);
        tv.tv_usec = 100;
        //重新注册event
        event_add(ce->ev, &tv);
    }else{
        printf("connect %d success and fd is %d \n", ce->port, SocketFD);
        instance->nodes[count_connected_nodes].fd = SocketFD;
        count_connected_nodes ++;
        if(count_nodes == count_connected_nodes){
            struct timeval *tv = malloc(sizeof(struct timeval));
            struct event *timeout = malloc(sizeof(struct event));
            event_set(timeout, SocketFD, 0, graber_leader, timeout);
            evutil_timerclear(tv);
            tv->tv_usec = 100;
            //重新注册event
            event_add(timeout, tv);
            event_del(ce->ev);
        }
    }
}


void graber_leader(int fd, short events, void* arg){
    char prepare_info[32];
    sprintf(prepare_info, "prepare %d", propose_id);
    printf("promise_propose_id %d \n", promise_propose_id);
    for(int i=0; i<instance->counts_nodes; i++){
        if(instance->nodes[i].id < 0){
            //未收到prepare信息
            if(promise_propose_id < 0){
                max_recive_propose_id = propose_id;
                if(str_equal(promise_propose, "NULL") == 1){
                    promise_propose_id = propose_id;
                }
                sprintf(instance->nodes[i].propose, "%s", propose);
                // 0表示可随意指定，即返回为NULL的情况
                instance->nodes[i].propose_id = 0;
            }
            //请求propose_id 小于 已接受的
            if(propose_id < promise_propose_id){
                instance->nodes[i].deny_p = 1;
                instance->nodes[i].propose_id = -1;
            }
            // 请求propose_id大于已接受的
            if(propose_id > promise_propose_id){
//                free(instance->nodes[i].propose);
                sprintf(instance->nodes[i].propose, "%s", propose);
                instance->nodes[i].propose_id = propose_id;
                if(str_equal(promise_propose, "NULL") == 1){
                    promise_propose_id = propose_id;
                }
                max_recive_propose_id = propose_id;
            }
            continue;
        }
        printf("graber send info %s \n", prepare_info);
        struct event *ev = malloc(sizeof(struct event));
        event_set(ev, instance->nodes[i].fd, EV_READ | EV_PERSIST, socket_read_cb, ev);
        event_add(ev, NULL);
        send_info(instance->nodes[i].fd, prepare_info);
    }
    struct event *timeout = arg;
    event_del(timeout);
}


struct hash_table* init_hash_table(long l){
    struct hash_table *ht = malloc(sizeof(struct hash_table));
    ht->length = l;
    ht->used = 0;
    struct string *nds = malloc(sizeof(string) * l);
    for(int i=0; i<l; i++){
        nds[i].key = NULL;
    }
    ht->node = nds;
    return ht;
}


struct Paxosinstance* new_paxos_instance(int num){
    instance->counts_nodes = num ;
    instance->uninit_node_index = 1;
    struct Nodes *nodes = malloc(sizeof(struct Nodes) * num);
    nodes[0].id = -1;
    nodes[0].fd = -1;
    nodes[0].deny_c = -1;
    nodes[0].deny_p = -1;
    nodes[0].propose_id = -1;
    nodes[0].propose = malloc(sizeof(char) * 32);
    nodes[0].role = malloc(sizeof(char) * 16);
    
    
    for(int i=1; i < num; i++){
        nodes[i].id = 0;
        nodes[i].fd = 0;
        nodes[i].deny_c = -1;
        nodes[i].deny_p = -1;
        nodes[i].propose_id = -1;
        nodes[i].propose = malloc(sizeof(char) * 32);
        nodes[i].role = malloc(sizeof(char) * 16);
    }
    instance->nodes = nodes;
    instance->commit_ack = malloc(sizeof(int));
    instance->prepare_ack_ids = malloc(sizeof(int));
    return instance;
}


void handle_nodes_fd(int fd){
    instance->nodes[count_connected_nodes].fd = fd;
}


int get_propose_id(){
    propose_id += propose_id_interval;
    while(max_recive_propose_id > propose_id){
        propose_id += propose_id_interval;
    }
    return propose_id;
}


void deal_prepare_null_ack(int fd, int proposer_id, char *propose){
    init_node(fd, proposer_id);
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].id ==proposer_id){
            instance->nodes[i].propose = propose;
            // 0表示可随意指定，即返回为NULL的情况
            instance->nodes[i].propose_id = 0;
        }
    }
    deal_prepare();
}


void deal_prepare_propose_ack(int fd, int proposer_id, int propose_id, char *propose){
    init_node(fd, proposer_id);
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].id ==proposer_id){
//            free(instance->nodes[i].propose);
            instance->nodes[i].propose = propose;
            instance->nodes[i].propose_id = propose_id;
        }
    }
    deal_prepare();
}


void deal_prepare_deny_p_ack(int fd, int proposer_id){
    init_node(fd, proposer_id);
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].id ==proposer_id){
            instance->nodes[i].deny_p = 1;
            instance->nodes[i].propose_id = -1;
        }
    }
    deal_prepare();
}


void deal_commit_deny_c_ack(int proposer_id){
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].id ==proposer_id){
            instance->nodes[i].propose_id = propose_id;
            instance->nodes[i].deny_c = 1;
        }
    }
    deal_commit();
}


void deal_commit_accepted_ack(int proposer_id){
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].id == proposer_id){
            instance->nodes[i].propose_id = propose_id;
            instance->nodes[i].deny_c = 0;
            instance->nodes[i].propose = propose;
        }
    }
    deal_commit();
}


void deal_prepare(){
    int max_propose_id = -1;
    int deny_p = 0;
    char *max_propose_propose = NULL;
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].deny_p == 1){
            deny_p += 1;
            continue;
        }
        //未全部返回直接return
        if(instance->nodes[i].propose_id == -1){
            return;
        }
        if(instance->nodes[i].propose_id >= max_propose_id && str_equal(instance->nodes[i].propose, "NULL") !=1){
            max_propose_id = instance->nodes[i].propose_id;
            max_propose_propose = instance->nodes[i].propose;
        }
    }
    
    if(max_propose_id <= 0){
        max_propose_id = propose_id;
        max_propose_propose = propose;
    }
    //有acceptor拒绝 或者全部返回拒绝
    if(deny_p >= (instance->counts_nodes + 1) / 2){
        prepare_agin();
        return;
    }
    char info[32];
    sprintf(info, "commit %d %s", propose_id, max_propose_propose);
    sprintf(propose, "%s", max_propose_propose);
    for(int i=0; i<instance->counts_nodes; i++){
        if(instance->nodes[i].deny_p != 1){
            if(instance->nodes[i].id > 0){
                printf("send info %s fd:%d \n", info, instance->nodes[i].fd);
                send_info(instance->nodes[i].fd, info);
            }
            if(instance->nodes[i].id < 0){
                // 被拒绝deny_c
                if(promise_propose_id > propose_id){
                    instance->nodes[i].propose_id = propose_id;
                    instance->nodes[i].deny_c = 1;
                }
                //被接受accepted
                else if(promise_propose_id <= propose_id){
                    instance->nodes[i].propose_id = propose_id;
                    instance->nodes[i].deny_c = 0;
                    sprintf(instance->nodes[i].propose, "%s", propose);
                    max_recive_propose_id = propose_id;
                    promise_propose_id = propose_id;
                    sprintf(promise_propose, "%s", propose);

                }
            }
        }
    }
    sprintf(success_propose, "%s", max_propose_propose);
    instance->status = "c";
    
}


int deal_commit(){
    int flag = 0;
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].deny_p != 1){
            if(instance->nodes[i].deny_c == -1){
                flag = -1;
                break;
            }
            if(instance->nodes[i].deny_c == 0){
                flag += 1;
            }
        }
        
        
    }
    // 返回信息未全部收回
    if(flag < 0){
        return 0;
    }
    
    //返回信息完全收回
    if(flag >= ((instance->counts_nodes + 1)/2)){
        printf("\033[31m learning la la la la .... propose %s \033[0m \n", success_propose);
        instance->status = "l";
        broad_to_nodes();
        deal_nodes_role(success_propose);
    }else{
        prepare_agin();
    }
    return 0;
}

void send_info(int fd, char *param_info){
    char info[strlen(param_info) + 1];
    sprintf(info, "%d,%s&", servPort, param_info);
    write(fd, info, strlen(info));
}


int init_node(int fd, int proposer_id){
    int is_init_flag = 0;
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].id == proposer_id ){
            is_init_flag = 1;
            instance->nodes[i].fd = fd;
            break;
        }
    }
    if(is_init_flag == 0){
        instance->nodes[instance->uninit_node_index].id = proposer_id;
        instance->nodes[instance->uninit_node_index].fd = fd;
        instance->nodes[instance->uninit_node_index].deny_c = -1;
        instance->nodes[instance->uninit_node_index].deny_p = -1;
        instance->nodes[instance->uninit_node_index].propose_id = -1;
        instance->uninit_node_index += 1;
    }
    return is_init_flag;
}


void prepare_agin(){
    instance->status = "p";
    for(int i=0; i < instance->counts_nodes; i++){
        
        instance->nodes[i].deny_c = -1;
        instance->nodes[i].deny_p = -1;
        instance->nodes[i].propose_id = -1;
        strcpy(instance->nodes[i].propose, "NULL");
    }
    get_propose_id();
    char prepare_info[32];
    sprintf(prepare_info, "prepare %d %s", propose_id, propose);
    sprintf(prepare_info, "prepare %d", propose_id);
    printf("again prepare propose_id %d %s\n", propose_id, prepare_info);
    for(int i=0; i<instance->counts_nodes; i++){
        if(instance->nodes[i].id < 0){
            //未收到prepare信息
            if(promise_propose_id < 0){
                max_recive_propose_id = propose_id;
                if(str_equal(promise_propose, "NULL") == 1){
                    promise_propose_id = propose_id;
                }
                strcpy(instance->nodes[i].propose, "NULL");
                // 0表示可随意指定，即返回为NULL的情况
                instance->nodes[i].propose_id = 0;
                continue;
            }
            //请求propose_id 小于 已接受的
            if(propose_id < promise_propose_id){
                instance->nodes[i].deny_p = 1;
                instance->nodes[i].propose_id = -1;
                continue;
            }
            // 请求propose_id大于已接受的
            if(propose_id > promise_propose_id){
                instance->nodes[i].propose_id = propose_id;
                if(str_equal(promise_propose, "NULL") == 1){
                    promise_propose_id = propose_id;
                }
                max_recive_propose_id = propose_id;
                strcpy(instance->nodes[i].propose, promise_propose);
                continue;
            }
        }
        send_info(instance->nodes[i].fd, prepare_info);
    }
}

void broad_to_nodes(){
    char info[32] = "\0";
    sprintf(info, "learning %s", success_propose);
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].id ==-1){
            deal_nodes_role(success_propose);
        }else{
            printf("send learning info: %s fd:%d \n", info, instance->nodes[i].fd);
            send_info(instance->nodes[i].fd, info);
        }
    }
}

void deal_nodes_role(char *learning_propose){
    int is_leader = 1;
    char **leader_info = malloc(sizeof(char) * 16 * 2);
    parse_io_stream(learning_propose, leader_info, "_");
    for(int i=0; i < instance->counts_nodes; i++){
        if(instance->nodes[i].id == atoi(leader_info[0])){
            is_leader = 0;
            sprintf(instance->nodes[i].role, "leader");
        }else{
            sprintf(instance->nodes[i].role, "slaver");
        }
    }
    leader = 0;
    if(is_leader == 1){
        printf("I'm the leader \n");
        leader = 1;
    }
}

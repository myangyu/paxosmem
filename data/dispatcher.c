#include<stdio.h>
#include "data/str.h"
#include "config.h"
#include<stdlib.h>
#include<string.h>
#include  <sys/socket.h>      /* basic socket definitions */
#include  <netinet/in.h>

#define STRING_MAX_SIZE  1000
#define STRING_MOD  STRING_MAX_SIZE


void set_key_value(int connfd, char *params[], int params_count, struct hash_table *ht){
    char *result = "success";
    set(ht, &params[1], &params[2]);
    int length = strlen(result);
    send(connfd, result, length, 0);
};


void get_key_value(int connfd, char *params[], int params_count, struct hash_table *ht){
    char *result = get(&params[1], ht);
    int length = strlen(result);
    send(connfd, result, length, 0);
};


void del_key(int connfd, char *params[], int params_count, struct hash_table *ht){
    char *result = "success";
    del(&params[1], ht);
    int length = strlen(result);
    send(connfd, result, length, 0);
};


int parse_io_stream(char info[], char **params){
    char* token = strtok(info, " ");
    int flag = 0;
    while( token != NULL )
    {
        params[flag] = token;
        token = strtok(NULL, " ");
        flag++;
    }
    return flag;
}


int parse_io_stream_paxos(char info[], char **params){
    char* token = strtok(info, "_");
    int flag = 0;
    while( token != NULL )
    {
        params[flag] = token;
        token = strtok(NULL, "_");
        flag++;
    }
    return flag;
}


void paxos_deal(int connfd, char *params[], int params_count, struct hash_table *ht){
    int prepare = 0;
    int commit = 0;
    int ret = 0;
    prepare = str_equal(params[2], "prepare");
    if(prepare == 1){
        send(connfd, "ack", 3, 0);
    }else{
        commit = str_equal(params[0], "commit");
        char **real_params = malloc(sizeof(char) * 8 * 4);
        params_count = parse_io_stream_paxos(params[3], real_params);
        ret = str_equal(real_params[0], "set");
        if(ret==1){
            set_key_value(connfd, real_params, params_count, ht);
        }
        ret = str_equal(real_params[0], "get");
        if(ret==1){
            get_key_value(connfd, real_params, params_count, ht);
        }
        ret = str_equal(real_params[0], "del");
        if(ret==1){
            del_key(connfd, real_params, params_count, ht);
        }
        send(connfd, "ack", 3, 0);
    }
}


void parse(char info[], struct funcs *f){
    int params_count = 0;
    int ret = 0;
    params_count = parse_io_stream(info, f->params);
    f->params_count = params_count;
    ret = str_equal(f->params[0], "paxos");
    if(ret==1){
        f->do_data = paxos_deal;
    }
    else{
        ret = str_equal(f->params[0], "set");
        if(ret==1){
            f->do_data = set_key_value;
        }
        ret = str_equal(f->params[0], "get");
        if(ret==1){
            f->do_data = get_key_value;
        }
        ret = str_equal(f->params[0], "del");
        if(ret==1){
            f->do_data = del_key;
        }
    }
}
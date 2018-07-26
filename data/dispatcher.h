/*************************************************************************
    > File Name: dispatcher.h
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2017年12月28日 20时00分10秒 CST
 ************************************************************************/
#include "data/str.h"

void set_key_value(int connfd, char *params[], int params_count, struct hash_table *);

void get_key_value(int connfd, char *params[], int params_count, struct hash_table *);

void del_key(int connfd, char *params[], int params_count, struct hash_table *);

void parse(char info[], struct funcs *f);

//void dispatcher(char info[], struct hash_table *ht, struct funcs *f);

int parse_io_stream(char info[], char **temp);

void set_key_value_paxos(int connfd, char *params[], int params_count, struct hash_table *ht);

void get_key_value_paxos(int connfd, char *params[], int params_count, struct hash_table *ht);

void del_key_paxos(int connfd, char *params[], int params_count, struct hash_table *ht);
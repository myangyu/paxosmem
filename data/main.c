/*************************************************************************
    > File Name: main.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2017年12月25日 18时31分14秒 CST
 ************************************************************************/

#include<stdio.h>
#include "str.h"
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<openssl/md5.h>
#include<stddef.h>

#define STRING_MAX_SIZE  100000
#define STRING_MOD  STRING_MAX_SIZE


void main(){
    struct hash_table *ht;
    ht = malloc(sizeof(hash_table));
    long l = STRING_MAX_SIZE;
    init_hash(l, ht);
    set(ht, "aaaa", "123");
    set(ht, "aaaa", "124");
    set(ht, "aaaa", "125");
    set(ht, "aaab", "121");
    printf("%s\n", get("aaaa", ht));
}

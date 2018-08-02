/*************************************************************************
    > File Name: string.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2017年12月15日 19时13分57秒 CST
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include <CommonCrypto/CommonCryptor.h>
#import <CommonCrypto/CommonDigest.h>
#include<stddef.h>
#include "str.h"

#define GET_ARRAY_LEN(array,len){len = (sizeof(array) / sizeof(array[0]));}


//初始化 string
void init(long length, struct string *nds){
    for(int i=0; i < length; i++){
        nds[i].key = "key";
        nds[i].val = "";
        nds[i].next = NULL;
    }
}

char hexChar(unsigned char c) {
    return c < 10 ? '0' + c : 'a' + c - 10;
}

void hexString32(unsigned char *from, char *to, int length) {
    for (int i = 0; i < length; ++i) {
        unsigned char c = from[i];
        unsigned char cHigh = c >> 4;
        unsigned char cLow = c & 0xf;
        to[2 * i] = hexChar(cHigh);
        to[2 * i + 1] = hexChar(cLow);
    }
    to[2 * length] = '\0';
}

//hash函数，将key进行hash
void hash_fun(char *key, char buf[]){
	unsigned char hash_index[CC_MD5_DIGEST_LENGTH];
    CC_MD5(key, (CC_LONG)strlen(key), hash_index);
    char hexResult32[2 * CC_MD5_DIGEST_LENGTH + 1];
    hexString32(hash_index, hexResult32, CC_MD5_DIGEST_LENGTH);
//    printf("md5: %s %d\n",hexResult32, CC_MD5_DIGEST_LENGTH);
    char tmp[3] = {'\0'};
	for(int i=0; i<CC_MD5_DIGEST_LENGTH; i++){
        sprintf(tmp,"%2.2x", hash_index[i]);
		strcat(buf, tmp);
	}
}


//将字符转换为int
int char_to_int(char c){
	char phexch[] = "ABCDEF";
	char qhexch[] = "abcdef";
	int i;
	for(i=0; i<6; i++){
        if((c == phexch[i]) || (c == qhexch[i]))
			break;
	}
	return 10 + i;
}


//将16进制字符串转化为整型
long long string_to_int(char *s){
	int len = 10;
	unsigned long long result=0;
	int i;
	for(i=0; i<len; i++){
		char temp = s[len-i-1];
		int tp = char_to_int(temp);
	    long base = pow(16,i);
        result += tp*base;
	}
    return result;
}


//判断字符串是否同
int str_equal(char a[], char b[]){
    int len_a = strlen(a);
    int len_b = strlen(b);
    int ret = 1;
    if(len_a!=len_b){
        ret = 0;
        return ret;
    }
    for(int i=0; i<len_a; i++){
        if(a[i]!=b[i]){
            ret = 0;
            break;
        }
    }
    return ret;

}


//初始化hash table
void init_hash(long length, struct hash_table *t){
    struct string *nds = malloc(sizeof(string) * length);
    t->node = nds;
}


struct string inner_set(struct hash_table *ht, char **key, char **value){
    printf("key %s mod: %s\n", *key, *value);
    char buf[33] ={'\0'};
	long long r;
	int mod;
	struct string *n = malloc(sizeof(string));
	struct string *node = ht->node;
	hash_fun(*key, buf);
	r = string_to_int(buf);
	n->key = *key;
	n->val = *value;
	n->next = NULL;
    mod = r % ht->length;
//    printf("key: %s  %s mod %d %lld\n", node[mod].key, *key, mod, r);
    if (node[mod].key == NULL){
	    node[mod] = *n;
	    ht->used += 1;
	}
	else{

	    struct string *temp = &node[mod];
	    while(temp != NULL){
            if(1==str_equal(temp->key, *key)){
                temp->val = *value;
                free(n);
                return *temp;
	        }
            if(temp->next == NULL){
                break;
            }else{
                temp = temp->next;
            }
        }
        temp->next = n;
        ht->used += 1;
    }
    return *n;
}


//rehash
void rehash(struct hash_table *ht){
    printf("rehash.....\n");
    long old_hash_length = ht->length;
    struct string *old_node = ht->node;
    struct string *new_node = malloc(sizeof(string) * old_hash_length *2);
    ht->node = new_node;
    ht->used = 0;
    ht->length *= 2;
    for(int i=0; i< old_hash_length; i++){
        struct string *header = &(old_node[i]);
        if(header->key == NULL){
            continue;
        }
        else{
            do{
                inner_set(ht, &(header->key), &(header->val));
                header = header->next;
                if(header == NULL){
                    break;
                }
            }while(header->key != NULL);
        }
    }
    free(old_node);
}


// 设置key， value
struct string set(struct hash_table *ht, char **key, char **value){
	struct string n;
	n = inner_set(ht, key, value);
    double cover_rate = (double)(ht->used) / (double)(ht->length);
//    todo rehash
//    if(cover_rate > 0.75){
//        printf("used :%ld %2.2f\n", ht->used, cover_rate);
//        rehash(ht);
//    };
    char *name = get(key, ht);
    return n;
}


//获取key的值
char *get(char **key, struct hash_table *ht){
    char buf[33] ={'\0'};
	long r;
	int mod;
	struct string n;
    struct string *temp;
	struct string *node = ht->node;
    int flag = 1;
	hash_fun(*key, buf);
	r = string_to_int(buf);
	mod = r % ht->length;
	n = node[mod];
	if(n.key == NULL){
	    return "nil";
	}
	if(str_equal(*key, n.key)){
	    return n.val;
	}
	while(flag == 1){
	    temp = node[mod].next;
	    if(temp == NULL){
	        return "nil";
	    }
	    if(str_equal(*key, temp->key)){
	        break;
	    }else{
	        temp = temp->next;
	    }
	}
	return temp->val;
}


char *del(char **key, struct hash_table *ht){

    char buf[33] ={'\0'};
	long r;
	int mod;
	struct string n;
    struct string *temp, *forward;
	struct string *node = ht->node;
    int flag = 1;

	hash_fun(*key, buf);
	r = string_to_int(buf);
	mod = r % ht->length;

	forward = &node[mod];
	temp = node[mod].next;
	if(forward->key == NULL){
	    return "success";
	}
	if(str_equal(*key, forward->key)){
	    if(temp !=NULL){
	        node[mod]= *temp;
	        free(forward);
	    }else{
	        forward->key = NULL;
	        forward->val = NULL;
	    }
	    return "success";
	}

	while(flag==1){
	    if(temp == NULL){
	        return "success";
	    }
	    if(!str_equal(*key, temp->key)){
	        forward = temp;
	        temp = temp->next;
	    }else{
            free(temp);
            forward->next = NULL;
            break;
	    }
	}
    temp = NULL;
    return "success";
}

//void main(){
//    struct hash_table *ht;
//    ht = malloc(sizeof(hash_table));
//    long l = 100;
//    ht->length = l;
//    ht->used = 0;
//
//    struct string *nds = malloc(sizeof(string) * l);
//    ht->node = nds;
//    char *keys[] = {
//        "aaa", "nnn","nnssaffn","aadfsnnn","nghdthgnn","nafnn","nafenn","nnfffn","nadaann",
//        "ythehnnn","nluolnn", "nnddnf","nnn","nnnxnbn","njfdnn","nasseenn","ndanadn","nnasadn",
//        "naadf","nafan","nasdn"
//    };
//    for(int i=0; i< 21; i++){
//        int temp = rand();
//        set(ht, &keys[i], &keys[i]);
//    }
//}

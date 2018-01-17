/*************************************************************************
    > File Name: test.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2017年12月18日 20时50分52秒 CST
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<openssl/md5.h>


struct string{
	int a;
	int b;
	struct string *next;
}string;

void hash_fun(char *key, char buf[]){
	unsigned char hash_index[16];
    MD5(key, strlen(key), hash_index);
	char tmp[3] = {'\0'};
  //  char buf[33] = {'\0'};

	for(int i=0; i<16; i++){
        sprintf(tmp,"%2.2x", hash_index[i]);
	//	printf("%2.2x \n", hash_index[i]);
		strcat(buf, tmp);
	int a = pow(2,3);
	}

// printf("result: %s", buf);
};


void test(){
	struct string *a;
	a = malloc(sizeof(string));
	a->a = 1;
	a->b = 2;
	a->next = NULL;
    printf("%d", a->a);
};

void point_to_list(char *p){
    printf("%d", p[3]);
    printf("%s", p);
};

void main(){
  //  char buf[33] = {'\0'};
  //  hash_fun("abcd", buf);
	//printf("%s", buf);
   //printf("%d", a);
    char *a = "abcde";
    point_to_list(a);
};

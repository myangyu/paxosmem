/*************************************************************************
    > File Name: t.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2017年12月27日 16时17分49秒 CST
 ************************************************************************/

#include<stdio.h>
struct string{
	char  *key;
	char  *val;
    struct string *next;
} string;


void main(){

	struct string *nds = malloc(sizeof(string) * 10);
	for(int i=0; i< 10; i++){
	
        printf("%s \n", nds[i]);
	}
}

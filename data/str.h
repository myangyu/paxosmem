/*************************************************************************
    > File Name: str.h
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2017年12月25日 18时01分03秒 CST
 ************************************************************************/

//#include<iostream.h>
//using namespace std;


struct string{
  char  *key;
  char  *val;
  struct string *next;
} string;


struct hash_table{

	long length;  //list长度
    long used; //已使用长度
	struct string *node; // hash list
} hash_table;

struct funcs{
    int params_count;
    void (*do_data)(int connfd, char *params[], int params_count, struct hash_table *);
    char **params;
}funcs;


void init(long length, struct string *nds);
void hash_fun(char *key, char buf[]);
int char_to_int(char c);
long long string_to_int(char *s);
int str_equal(char a[], char b[]);
int init_hash(long length, struct hash_table *t);
struct string inner_set(struct hash_table *ht, char **key, char **value);
void rehash(struct hash_table *ht);
struct string set(struct hash_table *ht, char **key, char **value);
char *get(char **key, struct hash_table *ht);
char *del(char **key, struct hash_table *ht);

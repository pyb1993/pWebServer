//
//  hash.h
//  pWenServer
//
//  Created by pyb on 2018/3/1.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef hash_h
#define hash_h
#include <stdio.h>
#include "pool.h"
#include "string_t.h"
#include "memory_pool.h"

#define char_hash(key, c)   ((uint) key * 31 + c)

#define HASH_ELT_SIZE(name)                                               \
(sizeof(void *) + align((name)->key.len + 2, sizeof(void *)))

typedef uint (*hash_key_pt) (u_char *data, size_t len);
typedef struct hash_slot{
    void             *value;    /* 指向用户自定义的数据 */
    u_short           len;      /* 键值key的长度 */
    u_char            name[1];  /* 键值key的第一个字符，数组名name表示指向键值key首地址 */
} hash_slot;

typedef struct hash {
    hash_slot  **buckets;  /* 指向hash散列表第一个存储元素的桶 */
    uint        size;     /* hash散列表的桶个数 */
} hash;

/* 计算待添加元素的hash元素结构 */
typedef struct {
    string         key;      /* 元素关键字 */
    uint        key_hash; /* 元素关键字key计算出的hash值 */
    void        *value;    /* 键-值<key，value> */
} hash_key;

typedef struct {
    hash       *hash;         /* 指向待初始化的基本hash结构 */
    hash_key_pt   key;          /* hash 函数指针 */
    
    uint        max_size;     /* hash表中桶bucket的最大个数 */
    uint        bucket_size;  /* 每个桶bucket的存储空间 */
    
    char        *name;         /* hash结构的名称(仅在错误日志中使用) */
    memory_pool *pool;         /* 分配hash结构的内存池 */
} hash_initializer;


void strlow(u_char *dst, u_char *src, size_t n);
uint hash_key_function(char *data, size_t len);
int hashInit(hash_initializer *hinit, hash_key *names, uint nelts);
void* hash_find(hash* h, char * name, size_t len);

#endif /* hash_h */

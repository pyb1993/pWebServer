//
//  connection.h
//  pWenServer
//
//  Created by pyb on 2018/3/23.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef connection_h
#define connection_h

#include <stdio.h>
#include "event.h"
#include "pool.h"
#include "memory_pool.h"
#include "string_t.h"

#define BUF_SIZE 1024

typedef struct buffer_s{
    char * begin;
    char * end;
    char * pos;
    char data[BUF_SIZE + 1];
    char limit[1];//用来指示buffer的最后一个位置
}buffer_t;


// 链接对象
// cpool用来分配内存
typedef struct connection {
    int fd;
    int side;
    void * data;//用来保存request
    event_t* rev;
    event_t* wev;
} connection_t;

// 链接池对象
typedef struct connection_pool_t{
    pool cpool;
} connection_pool_t;

#define buffer_append_cstring(buffer,cstr)(append_string_to_buffer(buffer,&STRING(cstr)))

connection_t* getIdleConnection();
void http_close_connection(connection_t* c);
void connectionPoolInit(int max_connections);
int buffer_recv(buffer_t*,int fd);
int append_string_to_buffer(buffer_t* buffer, const string* str);
int buffer_sprintf(buffer_t* buffer, const char* format,...);
buffer_t* createBuffer(memory_pool* pool);
void buffer_clear(buffer_t* buffer);
int buffer_send(buffer_t* buffer, int fd);
int buffer_size(buffer_t*buffer);
bool buffer_full(buffer_t* buffer);
#endif /* connection_h */

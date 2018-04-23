//
//  header.h
//  pWenServer
//
//  Created by pyb on 2018/4/16.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef header_h
#define header_h

#include <stdio.h>
#include "hash.h"
#include "connection.h"
#include "string_t.h"

#define COMM_HEADERs \
string date;                  \
string connection;

typedef struct http_request_s http_request_t;

typedef struct header_t{
    COMM_HEADERs
    string accept;
    string cookie;
} request_header_t;


// 实际上对应map_slot_t里面的void* val
typedef struct header_val_t {
    int offset;//用来对应是request_header里面的第几个header
    int (*header_parser) (http_request_t* r,int offset);// 对应的处理函数
} header_val_t;

// 对应初始化header的静态数组类型
typedef struct header_kv_t{
    string name;
    header_val_t val;
} header_kv_t;

void header_map_init();
int parse_header(http_request_t* r,buffer_t* b);
int header_handle_generic(http_request_t* r, int offset);
int header_handle_connection(http_request_t* r,int offset);
extern  hash* header_map;

#endif /* header_h */

//
//  config.h
//  pWenServer
//
//  Created by pyb on 2018/3/3.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef config_h
#define config_h



#include <stdio.h>
#include "vector.h"
#include "hash.h"

typedef struct config{
    int port;
    int worker_num;
    int daemon;
    int root_fd;
    int max_connections;
    int request_pool_size;
    int connection_pool_size;
    uint32_t timer_resolution;
    int post_accept_timeout;// 接受到读写请求之后,可以保持多少链接
    int keep_alive_timeout;// 一个keep alive的请求可以持续多久
    vector workers;
    hash* locations;// 用来需要转发的path
} config;


extern config server_cfg;

int config_load();





#endif /* config_h */

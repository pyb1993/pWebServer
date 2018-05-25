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

#define ROUND_MODE 0
#define CONSISTENT_HASH 1

typedef struct config{
    int port;
    int worker_num;
    int daemon;
    int root_fd;
    int max_connections;
    int request_pool_size;
    uint32_t timer_resolution;
    int post_accept_timeout;// 接受到读写请求之后,可以保持多少久
    int keep_alive_timeout;// 一个keep alive的请求可以持续多久
    int upstream_timeout;
    vector workers;
    hash* locations;// 设定了location => ip+port的映射
    void* loc_name_arr;//暂时 用来在全局传递域名
    int domain_num;// 需要转发的域名的个数
    int load_balance; // 负载均衡的模式
} config;


extern config server_cfg;

int config_load();





#endif /* config_h */

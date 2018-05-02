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
typedef struct config{
    int port;
    int worker_num;
    int daemon;
    int root_fd;
    int max_connections;
    int request_pool_size;
    int connection_pool_size;
    uint32_t timer_resolution;
    int post_accept_timeout;// 接受到请求后,可以保持多久链接
    vector workers;
} config;


extern config server_cfg;

int config_load();





#endif /* config_h */

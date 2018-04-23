//
//  config.c
//  pWenServer
//
//  Created by pyb on 2018/3/3.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "config.h"
#include "vector.h"
#include "worker.h"
#include <fcntl.h>


config server_cfg;

int config_load(){
    server_cfg.port = 3001;
    server_cfg.worker_num = 1;
    server_cfg.daemon = 1;
    server_cfg.max_connections = 1000;
    server_cfg.request_pool_size = 1024;
    server_cfg.connection_pool_size = 1024 * 3;
    server_cfg.root_fd = open("/Users/pyb/Documents/workspace/pWenServer/pWenServer", O_RDONLY);
    vectorInit(&server_cfg.workers,server_cfg.worker_num,sizeof(struct worker_t));
    return OK;
}

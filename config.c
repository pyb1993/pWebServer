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
#include "hash.h"
#include "http.h"
#include "commonUtil.h"
#include <fcntl.h>

#define MAX_LOCATION_SZIE 100
#define LOCATION(_host,_port) {.host = STRING(_host),.port = _port}


config server_cfg;
/*利用hash构造 header */

hash* create_location_map(){
    static string loc_name_arr[] = {
        STRING("/rails1"),
        STRING("/rails2"),
        STRING("/rails3"),
        STRING("/rails4"),
        STRING("/rails5")};
    
    static location_t locations[] = {
        LOCATION("127.0.0.1",3010),
        LOCATION("127.0.0.1",3011),
        LOCATION("127.0.0.1",3012),
        LOCATION("127.0.0.1",3013),
        LOCATION("127.0.0.1",3014)};

    static hash_key location_ele_array[MAX_LOCATION_SZIE];

    // 将header的东西转换成hashinit能够接受的参数
    int location_num = sizeof(locations)/sizeof(location_t);
    for(int i = 0; i < location_num; ++i){
        string name = loc_name_arr[i];
        location_ele_array[i].key = name;
        location_ele_array[i].key_hash = hash_key_function(name.c,name.len);
        location_ele_array[i].value = &locations[i];
    }
    
    // todo fix me:如果需要动态的加载配置,那么要考虑内存的释放,那么需要保留这个hash_initializer
    memory_pool *pool = createPool(2 * MAX_LOCATION_SZIE * HASH_ELT_SIZE(&location_ele_array[0]));
    hash * h = (hash*) pMalloc(pool, sizeof(hash));
    hash_initializer localtion_hash_init;
    localtion_hash_init.pool = pool;
    localtion_hash_init.hash = h;
    localtion_hash_init.bucket_size = 128;
    localtion_hash_init.max_size = 1024;
    int ret = hashInit(&localtion_hash_init, location_ele_array,location_num);
    ABORT_ON(ret == ERROR, "init header failed!!!");
    return localtion_hash_init.hash;
}

int config_load(){
    server_cfg.port = 3001;
    server_cfg.worker_num = 1;
    server_cfg.daemon = 1;
    server_cfg.max_connections = 1000;
    server_cfg.request_pool_size = 2048 + 1024;
    server_cfg.connection_pool_size = 1024 * 3;
    server_cfg.post_accept_timeout = 0 * 1000;// standard: 15s
    server_cfg.keep_alive_timeout = 100 * 1000;// keep alive 100ms
    server_cfg.root_fd = open("/Users/pyb/Documents/workspace/pWebServer/pWebServer", O_RDONLY);
    server_cfg.timer_resolution = 500;
    
    // 初始化locations
    server_cfg.locations = create_location_map();
    vectorInit(&server_cfg.workers,server_cfg.worker_num,sizeof(struct worker_t));
    return OK;
}

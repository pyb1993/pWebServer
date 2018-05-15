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
#define LOCATION(_host,_port,_w) {.host = STRING(_host),.port = _port,.weight = _w,.max_fails = 1,.fail_timedout = 10 * 1000}


config server_cfg;
/*利用hash构造 header */

hash* create_location_map(){
    static string loc_name_arr[] = {
        STRING("rails/1"),
        STRING("rails/2"),
        STRING("rails/3"),
    };
    
    // 认为不同的端口对应着不同的服务,weight设置的不一样
    static location_t locations_1[] = {
        LOCATION("127.0.0.1",3010,2),
        LOCATION("127.0.0.1",3011,3),
        LOCATION("127.0.0.1",3012,4),
        LOCATION("127.0.0.1",3013,5),
        LOCATION("127.0.0.1",3014,6)};
    
    static location_t locations_2[] = {
        LOCATION("127.0.0.1",3015,1),
        LOCATION("127.0.0.1",3016,3),
        LOCATION("127.0.0.1",3017,2),
        LOCATION("127.0.0.1",3018,4),
        LOCATION("127.0.0.1",3019,5)};
    
    static location_t locations_3[] = {
        LOCATION("127.0.0.1",3020,2),
        LOCATION("127.0.0.1",3021,3),
        LOCATION("127.0.0.1",3022,4),
        LOCATION("127.0.0.1",3023,1),
        LOCATION("127.0.0.1",3024,7)};
    
    static location_t* locations[3] = {locations_1,locations_2,locations_3};

    static hash_key location_ele_array[MAX_LOCATION_SZIE];
    
    // 将header的东西转换成hashinit能够接受的参数
    int location_num = sizeof(loc_name_arr)/sizeof(string);
    
    server_cfg.domain_num = location_num;
    server_cfg.loc_name_arr = loc_name_arr;
    
    for(int i = 0; i < location_num; ++i){
        string name = loc_name_arr[i];
        location_ele_array[i].key = name;
        location_ele_array[i].key_hash = hash_key_function(name.c,name.len);
        location_ele_array[i].value = locations[i] ;
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
#undef locations_arr
}

int config_load(){
    server_cfg.port = 3001;
    server_cfg.worker_num = 1;
    server_cfg.daemon = 1;
    server_cfg.max_connections = 4096;
    server_cfg.request_pool_size = 4096;
    server_cfg.upstream_timeout = 8 * 1000;// 8 s
    server_cfg.post_accept_timeout = 8 * 1000;// standard: 15s
    server_cfg.keep_alive_timeout = 1000 * 1000;// keep alive 30s
    server_cfg.root_fd = open("/Users/pyb/Documents/workspace/pWebServer/pWebServer", O_RDONLY);
    server_cfg.timer_resolution = 500;
    server_cfg.load_balance = ROUND_MODE;
    // 初始化locations
    server_cfg.locations = create_location_map();
    vectorInit(&server_cfg.workers,server_cfg.worker_num,sizeof(struct worker_t));
    return OK;
}

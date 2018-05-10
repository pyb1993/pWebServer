//
//  upstream_server_module.c
//  pWebServer
//
//  Created by pyb on 2018/5/9.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "upstream_server_module.h"
#include "http.h"
#include "commonUtil.h"
#include "connection.h"

domain_upstream_server_arr_t servers;

/*upstream 加权轮询模块通用context*/
upsream_server_module_t upstream_server_round_module_ctx = {
    get_server_by_round,
    free_server_after_round_select
};

/*upstream 一致性hash模块通用context*/
upsream_server_module_t upstream_server_chash_module_ctx = {
    NULL
};

/*  在本次加权轮询之前挑选之前进行初始化
    创造一个位图bit_map:用来确定本轮挑选是否被访问过
    可能需要分配对应的内存空间
 */
int init_before_round(http_request_t* r, string* target_domain,upsream_server_arr_t** target_server_of_domain){
    upsream_server_arr_t* servers_of_domain = servers.server_ip_arr;
    int n = servers.nelts;
    // 找到对应的域名, todo: hash实现
    int i = 0;
    for(i = 0; i < n; ++i){
        servers_of_domain++;
        if( stringEq(target_domain, servers_of_domain->domain_name)){
            break;
        }
    }
    ERR_ON(i >= n, "?????");

    //首先分配位图的空间
    int ip_num = servers_of_domain->nelts;
    if(ip_num <= 8 * sizeof(uintptr_t)){
        servers_of_domain->tried = &servers_of_domain->data;
        servers_of_domain->data = 0;
    }else{
        int n = (ip_num + (8 * sizeof(uintptr_t) - 1)) / (8 * sizeof(uintptr_t));//向上取整,分配的基本单位是8 * 4 = 32byte
        servers_of_domain->tried = (uintptr_t*)pMalloc(r->pool, n * sizeof(uintptr_t));
        if (servers_of_domain->tried == NULL) {
            return ERROR;
        }
    }
    
    // 然后初始化这些数组
    upsream_server_t* server_arr = servers_of_domain->upstream_server;
    for(int j = 0; j < ip_num;++j){
    // 设置对应的状态
        server_arr->state = UPSTREAM_CONN_SUC;
        server_arr++;
    }
    // 将最终找到的结果保存起来,供外界使用
    *target_server_of_domain = servers_of_domain;
    return OK;
}

/*
 在挑选一次之后,需要尝试链接该服务器
 */
int try_connect(upsream_server_t* us){
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        ERR_ON(fd == -1, "try connecting upstream server failed");
        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(us->location->port);
        int success = inet_pton(AF_INET, us->location->host.c, &addr.sin_addr);
        if (success <= 0) {
            return -1;
        }
        
        int err = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
        if (err < 0) {
            return -1;
        }
    return fd;
}


/*  按照加权轮训算法来进行挑选一个服务器,一次挑选,如果本次失败,那么会记录在位图里面
 */
upsream_server_t* get_server_by_round_once(upsream_server_arr_t* server_domain){
    int n = 0;
    int i = 0;
    uintptr_t m = 0;
    int total = 0;
    uintptr_t* tried = server_domain->tried;
    
    upsream_server_t* best = NULL;
    
    for(i = 0; i < server_domain->nelts; ++i){
        /* 计算当前后端服务器在位图中的位置 n,以及n那个位置的m */
        
        n = i / (8 * sizeof(uintptr_t));
        m = (uintptr_t) (1 << i % (8 * sizeof(uintptr_t)));
        
        /* 当前后端服务器在位图中已经有记录，则不再次被选择，即 continue 检查下一个后端服务器 */
        if (tried[n] & m) {
            continue;
        }
        
        /*下面检查这个server的情况,假如设定了最大失败次数,且失败的次数超过了它
          且距离检查的时间还没有吵过等待时间,那么就直接忽略这个服务器
         */
        
        upsream_server_t* server = server_domain->upstream_server + i;
        if (server->max_fails
            && server->fails >= server->max_fails
            && (int)(current_msec - server->checked) <= (int)server->fail_timeout)
        {
            continue;
        }
        
        // 接下来就是核心算法,平滑加权轮询问
        server->current_weight += server->effective_weight;
        total += server->effective_weight;
        
        /* 服务器正常，如果effective_weight比较小(说明中间失败过),那么现在慢慢 effective_weight 的值,相当于缓慢恢复 */
        if (server->effective_weight < server->weight) {
            server->effective_weight++;
        }
        
        /* 选择current_weight最大的作为best */
        if (best == NULL || server->current_weight > best->current_weight) {
            best = server;
        }
    }
    
    if (best == NULL) {
        return NULL;
    }
    
    i = (int)(best - server_domain->upstream_server);//计算best是第几个
    
    /* 在位图相应的位置记录被选中后端服务器 */
    tried[n] |= m;
    
    /* 更新被选中后端服务器的权重,理解这里为什么要这样做,这样可以根据effective_weight的大小,决定大概多少次就能再次被选中 */
    best->current_weight -= total;
    
    /* 如果距离上一次check已经超过了fail_timeout时间,那么就应该更新
       这里要理解的是:在fail_timeout时间之内,我们不去更新checked(保留的是一段fail_timeout时间的起点)
     */
    if ((int)(current_msec - best->checked) > (int)best->fail_timeout) {
        best->checked = current_msec;
    }
    return best;
};

/*需要在链接完成之后,释放整个服务器*/
void free_server_after_round_select(upsream_server_t* us){
 
    /*
     * 若在本轮被选中的后端服务器在连接时失败
     * 则需要进行重新选择后端服务器
     */
    
    if (us->state == UPSTREAM_CONN_FAIL) {
    
        us->fails++;/* 增加当前后端服务器失败的次数 */
        /* 设置当前后端服务器访问的时间,其实就是上一次失败的时间 */
        us->checked = current_msec;
        
        if (us->max_fails) {
            /* 由于当前后端服务器失败，表示发生异常，此时降低 effective_weight 的值 */
            us->effective_weight -= us->weight / us->max_fails;
        }
        
        /* 保证 effective_weight 的值不能小于 0 */
        if (us->effective_weight < 0) {
            us->effective_weight = 0;
        }
    }else{
        /* 若被选中的后端服务器成功处理请求,并且已经超过fail timeout了，则将其 fails 设置为 0
           这里需要理解的是,在fail_timeout时间之内,是不会清空fails的
         */
        if ( (int)(current_msec - us->checked) <= (int)us->fail_timeout) {
            us->fails = 0;
        }
    }
}

/*
 按照加权轮询算法来挑选一个服务器,并且测试该链接是否可用
 如果测试成功,那么直接把对应的fd返回给前面
 如果测试失败,那么继续挑选
 */
int get_server_by_round(http_request_t*r,string* domain){
    upsream_server_arr_t* server_domain;
    int err = init_before_round(r, domain,&server_domain);
    if(err != OK){
        plog("err on init load balance");
        return -1;
    }
    
    int tries = 0; //尝试的次数
    while(tries < server_domain->nelts){
        upsream_server_t* us = get_server_by_round_once(server_domain);
        int fd = try_connect(us);
        if(fd > 0) {return fd;}
        else{
            //测试失败,那么需要重新尝试
            us->state = UPSTREAM_CONN_FAIL;
            free_server_after_round_select(us);
        }
        plog("err on connec backend,fd:%d",r->connection->fd);
        tries++;
    }
    return -1;
}



//
//  p_event.c
//  pWenServer
//
//  Created by pyb on 2018/3/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "module.h"
#include "globals.h"
#include "server.h"
#include "kqueue_event_module.h"
#include "upstream_server_module.h"
#include "connection.h"
#include "http.h"
#include "commonUtil.h"
#include <sys/time.h>

/*对应的共享内存的全局变量*/
shmtx_t   accept_mutex;

/*******构建event_module***************/
event_actions_t event_actions;


/*事件模块通用接口的定义*/
event_module_t event_module_ctx = {
    STRING("event module context"),
    NULL,// create conf
    NULL,// init conf
    {}
};

 /******************************/
module_t event_module = {
    STRING("event module"),
    &event_module_ctx,                  /* module context */
    &event_process_init,               /* init process */
    &event_module_init,                             /* init module */
};


module_t upstream_module = {
    STRING("upstream module"),
    NULL,
    &upstream_process_init,
    &upstream_module_init
};


// 用来在worker启动的时候,初始化upstream的相关对象
int upstream_process_init(){
    //首先要初始化servers
    servers.nelts = server_cfg.domain_num;
    servers.server_ip_arr = (upsream_server_arr_t*)malloc(sizeof(upsream_server_arr_t) * servers.nelts);
    for(int i = 0; i < servers.nelts; ++i){
        upsream_server_arr_t* server_arr = servers.server_ip_arr + i;
        server_arr->nelts = 10;// 这里写死; todo以后依靠配置文件来定
        server_arr->init_state = IDLE;//写好初始化状态
        server_arr->upstream_server = (upsream_server_t*)malloc(server_arr->nelts * sizeof(upsream_server_t));
        string* domain_name = (string*)server_cfg.loc_name_arr + i;
        server_arr->domain_name = domain_name;
        server_arr->init_state = IDLE;
        location_t* loc = hash_find(server_cfg.locations, domain_name->c, domain_name->len);// 获取到loc对象数组,并用这个数组来初始化下面的对象
        for(int j = 0; j < server_arr->nelts; ++j){
            // 初始化每一个 ip对应的数据
            location_t* one_server_loc = loc + j;
            upsream_server_t* upstream_server = server_arr->upstream_server + j;
            upstream_server->effective_weight = one_server_loc->weight;
            upstream_server->weight = one_server_loc->weight;
            upstream_server->location = one_server_loc;
            upstream_server->current_weight = 0;
            upstream_server->status_changed = 0;
            upstream_server->max_fails = (server_cfg.load_balance == ROUND_MODE) ? 1 : 3;
            upstream_server->fails = 0;
            upstream_server->fail_timeout = one_server_loc->fail_timedout;
        }
    }
    
    // 接下来要初始化几个结构,根据负载均衡参数设置的不同,选择不一样的context
    if(server_cfg.load_balance == ROUND_MODE){
        plog("round load balance module is selected\n");
        upstream_module.ctx = &upstream_server_round_module_ctx;
    }else if(server_cfg.load_balance == CONSISTENT_HASH){
        plog("consistent hash load balance module is selected\n");
        upstream_module.ctx = &upstream_server_chash_module_ctx;
    }
    return OK;
};

/*在所有进程启动之前运行的函数*/
int upstream_module_init(){
    return OK;
};




/*目前来说,暂时只进行了共享内存的分配
  和具体模块类型没有关系*/
int event_module_init(){
    size_t               size, cl;
    shm_t shm;// 本地分配的一个变量
    // 要大于或等待cache line
    cl = 128;
    size = cl            /* ngx_accept_mutex */
    + cl          /* ngx_connection_counter */
    + cl;         /* ngx_temp_number */
    
    shm.size = size;
    shm.name.len = sizeof("nginx_shared_zone");
    shm.name.c = "nginx_shared_zone";
    
    // 创建共享内存
    if (shm_alloc(&shm) != OK) {
        return ERROR;
    }
    // shm为创建的共享内存
    // 因为之前有预留128字节，所以shared指向它是没有问题的
    void* shared = shm.addr;
    
    // 这个锁不进行自旋
    accept_mutex.spin = (uint) -1;

    // 将accept_mutex的locked指向shared
    if (shmtx_create(&accept_mutex, shared)
        != OK){
        return ERROR;
    }
    return OK;
}



// 用来在每一个worker开始的时候进行初始化,通过对应的配置来选择对应的事件驱动模型
// 这个connection对应着多一个读事件
int event_process_init()
{
    // 根据不同的事件模型进行初始化
    if(1)
    {
        // 选择对应的actions
        plog("kqueue event module is selected\n");
        kqueue_module.process_init();
    }
    else{
        return ERROR;
    }

    
    // 初始化事件集合
    r_events = malloc(sizeof(event_t) * (server_cfg.max_connections + 10));
    w_events = malloc(sizeof(event_t) * (server_cfg.max_connections + 10));
    
    // 初始化链接池
    connectionPoolInit(server_cfg.max_connections);
    
    // 对listenning fd进行监听,分配对应的链接,和事件
    connection_t* c = getIdleConnection();
    c->fd = listen_fd;
    
    // 设置读事件和链接的关系
    event_t* rev = c->rev;
    
    //设置连接回调，当有客户端连接时，将触发回调
    rev->handler = event_accept;
    
    //如果work进程之间没有使用锁，则把读事件加入epoll中
    //此时写事件的回调为NULL，因为在ngx_get_idle_connection函数中会把整个结构进行清0操作
    if (add_event(rev, READ_EVENT, 0) == ERROR)
    {
        plog("add listenning to eventloop failed!!!");
        return ERROR;
    }
    
    // 初始化timer
    uint32_t timer_resolution = server_cfg.timer_resolution;
    if (timer_resolution) {
        struct sigaction  sa;
        struct itimerval  itv;
        
        memzero(&sa, sizeof(struct sigaction));
        
        /* 指定信号处理函数 */
        sa.sa_handler = timer_signal_handler;// p_event_timer_alarm = 1
        /* 初始化信号集 */
        sigemptyset(&sa.sa_mask);
        
        /* 捕获信号SIGALRM */
        if (sigaction(SIGALRM, &sa, NULL) == -1) {
            plog("error on sigaction on sigalrm");
            return ERROR;
        }
        
        /* 设置时间精度 */
        itv.it_interval.tv_sec = timer_resolution / 1000;
        itv.it_interval.tv_usec = (timer_resolution % 1000) * 1000;
        itv.it_value.tv_sec = timer_resolution / 1000;
        itv.it_value.tv_usec = (timer_resolution % 1000 ) * 1000;
        
        /* 使用settimer函数发送信号 SIGALRM,ITIMER_REAL(系统的真实时间,不是在用户进程下的时间。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                ) */
        if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
            plog("setitimer() failed");
        }
    }
    return OK;
}

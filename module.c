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
#include "connection.h"
#include "http.h"
#include "commonUtil.h"
#include <sys/time.h>

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
    NULL,                             /* init module */
};


// listenning socket的回调函数
/*
 需要取出所有的fd
 
 这里存在的一个问题是,在事件A里面释放事件C的链接,但是事件B又申请了一个新链接,这就导致事件
 C将会把链接搞混(单纯的把fd变成-1解决不了这个问题),需要通过instance来进行取反判定。但是
 如果同时出现事件B,B'也会造成两次取反,所以还有一个做法是将accept的事件先执行,其他事件后面执行
 就不会导致链接出问题了
 */


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





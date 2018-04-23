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


void event_accept(event_t *ev)
{
    int socklen = sizeof(struct sockaddr_in);
    connection_t* listenning = ev->data;
    
    while(1)
    {
        //接收客户端连接
        //需要检查是否超时,如果已经超时了,那么就应该做出相应的处理
        int conn_fd = accept(listenning->fd,NULL,NULL);//这里是可能阻塞的
        if(conn_fd == -1){
            ERR_ON((errno != EWOULDBLOCK), "accept");
            break;
        }
        else{
            plog("accept connection %d", conn_fd);
        }

        //获取一个空闲连接对象
        //存在的问题在于,如果客户端不发送实际的数据,那么就会平白分配内存
        connection_t* c = getIdleConnection();
        
        //给新连接对象赋值
        c->fd = conn_fd;
        c->side = C_DIRECTSTREAM;
    
        http_init_connection(c);      //ngx_http_init_connection
    }
}


// 用来在每一个worker开始的时候进行初始化,通过对应的配置来选择对应的事件驱动模型
// 这个connection对应着多一个读事件
int event_process_init()
{
    if(1)
    {
        // 选择对应的actions
        plog("kqueue event module is selected\n");
        event_actions = ((event_module_t*)kqueue_module.ctx)->actions;
        event_actions.event_init();// 初始化事件相关
        // 初始化timer
        
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
        
        //如果work进程之间没有使用枷锁，则把读事件加入epoll中
        //此时写事件的回调为NULL，因为在ngx_get_connection函数中会把整个结构进行清0操作
        if (add_event(rev, READ_EVENT, 0) == ERROR)
        {
            plog("add listenning to eventloop failed!!!");
            return ERROR;
        }
    }
    else{
        return ERROR;
    }

    return OK;
}





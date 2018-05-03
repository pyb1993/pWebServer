//
//  p_event.h
//  pWenServer
//
//  Created by pyb on 2018/3/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef p_event_h
#define p_event_h

#include <stdio.h>
#include "event.h"
#include "string_t.h"
#include <sys/socket.h>
#include <netinet/in.h>

int event_process_init();

typedef struct p_module{
    string name;
    void *ctx;//指向这一类模块的通用接口结构体,用来在解析配置的时候进行各种初始化
    int (*process_init)();//在worker初始化的时候需要调用这个函数
    int(*module_init)();//在所有模块初始化的时候调用这个函数,主要处理master关于负载均衡,共享内存的共享锁等数据,暂时没有
} module_t;

/****event actions t结构,用来定义相应事件模块的所有事件操作的方法***********/
typedef struct p_event_actions{
    int (*p_add_event)(event_t *ev, int event, uint flags);
    int (*p_del_event)(event_t *ev, int event, uint flags);
    int (*event_init)();//在事件模块启动的时候进行初始化
    void (*process_events)(msec_t timer,int flags);//在事件循环的时候等待
} event_actions_t;

/*event模块的上下文(通用接口)*/
typedef struct {
    /* 事件模块名称 */
    string  name;
    void (*createConf)();
    void (*initConf)();
    /* 每个事件模块具体实现的方法，有10个方法，即IO多路复用模型的统一接口 */
    event_actions_t     actions;
} event_module_t;

extern module_t event_module;
extern event_actions_t event_actions;
#endif /* p_event_h */

//
//  kqueue_event_module.c
//  pWenServer
//
//  Created by pyb on 2018/3/27.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "globals.h"
#include "module.h"
#include "event.h"
#include <sys/event.h>
#include <errno.h>
#include <stdlib.h>
#include "kqueue_event_module.h"
#include "commonUtil.h"
#include "connection.h"

#define ADD_EVENT 0
#define DEL_EVENT 1
#define CLOSE_EVENT 2

static int kq = -1;

int kequeue_event_init(){
    kq = kqueue();
    if(kq == -1){
        ABORT_ON(kq == -1, "kqueue init failed!");
        return ERROR;
    }
    else{
        plog("kqueue init success!");
        return OK;
    }
}


// kqueue模块用来添加事件的函数
int kqueue_add_event (event_t *ev, int event, uint flags)
{
    
    if(ev->active) {return OK;}
    ev->active = true;
    connection_t* c = ev->data;
    int fd = c->fd;
    /* events参数是方便下面确定当前事件是可读还是可写 */
    uint32_t events = (uint32_t) event;
    struct kevent changes[1];
    EV_SET(&changes[0], fd, events, EV_ADD, 0, 0, c);
    
    int ret = kevent(kq, changes, 1, NULL, 0, NULL);
    if(ret == -1){
        ERR_ON(ret == -1, "error on kqueue add event");
        return ERROR;
    }
    return OK;
}


// kqueue模块用来删除事件的函数
// todo 可以用flags来进行优化
// 这里是需要ev的状态的,因为需要判断对应的事件是否还存在在事件循环机制里面,避免不必要的删除
int kqueue_del_event (event_t *ev, int event, uint flags)
{
    // todo Nginx做的优化
    if(!ev->active){return OK;}
    ev->active = false;
    
    connection_t* c = ev->data;
    int fd = c->fd;


    uint32_t events = (uint32_t) event;
    struct kevent changes[1];
    EV_SET(&changes[0], fd, events, EV_DELETE, 0, 0, c);
    int ret = kevent(kq, changes, 1, NULL, 0, NULL);
    if(ret == -1){
        ERR_ON(ret == -1, "error on kqueue del event");
        return ERROR;
    }
    return OK;
}


// 进行事件循环并且处理
void kqueue_process_events(){
    struct kevent events[MAX_EVENT_NUM];

    while(1){
        int n = kevent(kq, NULL, 0, events, MAX_EVENT_NUM, NULL);
        if(n == -1){
            if(errno != EINTR)
            {
                plog("error on kevent loop %d",errno);
            }
            continue;
        }
        for(int i = 0; i < n; ++i){
            struct kevent event = events[i];
            if(event.filter == EVFILT_READ){
                // 处理读的事件
                connection_t* c = event.udata;
                event_t* rev = c->rev;
                if(rev->handler){
                    rev->handler(rev);
                }
            }
            else if(event.filter == EVFILT_WRITE) {
                connection_t* c = event.udata;
                event_t* wev = c->wev;
                if(wev->handler){
                    wev->handler(wev);
                }
            }
        }
    }
}


/***** kequeue module结构************/
event_module_t kqueue_module_ctx = {
    STRING("kqueue module context"),
    NULL,
    NULL,
    {
        // keuque的各种事件相关函数
        &kqueue_add_event,
        &kqueue_del_event,
        &kequeue_event_init,
        &kqueue_process_events
    }
};

module_t kqueue_module = {
    STRING("kqueue module"),
    &kqueue_module_ctx,
    NULL,
    NULL
};

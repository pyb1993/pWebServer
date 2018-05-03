//
//  event.h
//  pWenServer
//
//  Created by pyb on 2018/3/27.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef event_h
#define event_h
#include <stdio.h>
#include "globals.h"
#include <sys/types.h>
#include <sys/event.h>
#include "rb_tree.h"

#if (MACOS)
    #define READ_EVENT     EVFILT_READ
    #define WRITE_EVENT    EVFILT_WRITE
#endif

# define add_event event_actions.p_add_event
# define del_event event_actions.p_del_event
# define event_process event_actions.process_events

typedef struct event_s event_t;
typedef void (*event_handler_pt)(event_t *ev);

/* 红黑树的哨兵节点 */
extern rbtree_t  event_timer_rbtree;
extern rbtree_node_t  event_timer_sentinel;
extern int current_msec;


typedef rbtree_key_t  msec_t;
typedef rbtree_key_int_t msec_int_t;

// 事件结构体
struct event_s{
    void *data;// 指向connection
    event_handler_pt  handler;
    bool active; // 标志是否是活跃的事件(用来避免重复添加事件)
    bool timer_set;//标志是否在红黑树里面
    bool timedout;
    rbtree_node_t   timer;
    // timer的结构
};

int event_timer_init();
void event_accept(event_t *ev);
void event_del_timer(event_t *ev);
void event_add_timer(event_t *ev, msec_t timer);
void event_expire_timers(void);
msec_t event_find_timer(void);

void time_update();//更新缓存时间
void timer_signal_handler(int signo);





#endif /* event_h */

//
//  timer_event.c
//  pWebServer
//
//  Created by pyb on 2018/4/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "commonUtil.h"
#include "event.h"
#include "worker.h"

 rbtree_t  event_timer_rbtree;
/* 红黑树的哨兵节点 */
rbtree_node_t  event_timer_sentinel = {
    .parent = &event_timer_sentinel,
    .left = &event_timer_sentinel,
    .right = &event_timer_sentinel,
    .color = 0
};
int current_msec = 0;

/* 定时器事件初始化 */
int event_timer_init()
{
    /* 初始化红黑树 */
    rbtree_init(&event_timer_rbtree, &event_timer_sentinel,
                    rbtree_insert_timer_value);
    return OK;
}

/* 从定时器中移除事件 */
void event_del_timer(event_t *ev)
{
    /* 从红黑树中移除指定事件的节点对象 */
    rbtree_delete(&event_timer_rbtree, &ev->timer);
    /* 设置相应的标志位 */
    ev->timer_set = 0;
}


/* 将事件添加到定时器中 */
void event_add_timer(event_t *ev, msec_t timer)
{

    msec_t      key;
    msec_t  diff;
    
    /* 设置事件对象节点的键值 */
    key = current_msec + timer;
    
    /* 判断事件的相应标志位 */
    if (ev->timer_set) {
        
        /*如果该事件已经在定时器里面了,那么如果时间相差不远,就直接忽略这次add操作,提高性能*/
        
        diff = (msec_t) (key - ev->timer.key);
        
        if (p_abs(diff) < TIMER_LAZY_DELAY) {
            return;
        }
        event_del_timer(ev);
    }
    
    ev->timer.key = key;
    
    
    /* 将事件对象节点插入到红黑树中 */
    rbtree_insert(&event_timer_rbtree, &ev->timer);
    
    
    /* 设置标志位 */
    ev->timer_set = 1;
}

/*找到一个最容易超时的事件对象*/
msec_t event_find_timer(void)
{
    msec_int_t      timer;
    rbtree_node_t  *node, *root, *sentinel;
    
    /* 若红黑树为空,则返回一个最大的超时时间 */
    if (event_timer_rbtree.root == &event_timer_sentinel) {
        return TIMER_INFINITE;
    }
    
    root = event_timer_rbtree.root;
    sentinel = event_timer_rbtree.sentinel;
    
    /* 找出红黑树最小的节点，即最左边的节点 */
    node = rbtree_min(root, sentinel);
    
    
    /* 计算最左节点键值与当前时间的差值timer，当timer大于0表示不超时，不大于0表示超时 */
    timer = (msec_int_t) (node->key - current_msec);
    
    /*
     * 若timer大于0，则事件不超时，返回该值；
     * 若timer不大于0，则事件超时，返回0，标志触发超时事件；
     */
    return (msec_t) (timer > 0 ? timer : 0);
}

/* 检查定时器中所有事件,处理所有的超时事件 */
void event_expire_timers(void)
{
    event_t        *ev;
    rbtree_node_t  *node, *root, *sentinel;
    
    sentinel = event_timer_rbtree.sentinel;
    
    /* 循环检查 */
    while(1) {
        root = event_timer_rbtree.root;
        
        /* 若定时器红黑树为空，则直接返回，不做任何处理 */
        if (root == sentinel) {
            return;
        }
        
        /* 找出定时器红黑树最左边的节点，即最小的节点，同时也是最有可能超时的事件对象 */
        node = rbtree_min(root, sentinel);
        
        
        /* 若检查到的当前事件已超时 */
        
        if (is_expired(node->key,current_msec)) {
            /* 获取超时的具体事件 */
            ev = (event_t *) ((char *) node - offsetof(event_t, timer));
            rbtree_delete(&event_timer_rbtree, &ev->timer);
            /* 设置事件的在定时器红黑树中的监控标志位 */
            ev->timer_set = 0;/* 0表示不受监控 */
            /* 设置事件的超时标志位 */
            ev->timedout = 1;/* 1表示已经超时 */
            
            /* 调用已超时事件的处理函数对该事件进行处理,因为已经超时了,所以必须马上处理 */
            ev->handler(ev);
            continue;
        }
        
        break;
    }
    
}

 void timer_signal_handler(int signo)
 {
     p_event_timer_alarm = 1;
 }

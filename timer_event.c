//
//  timer_event.c
//  pWebServer
//
//  Created by pyb on 2018/4/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "timer_event.h"
 rbtree_t  event_timer_rbtree;
/* 红黑树的哨兵节点 */
rbtree_node_t  event_timer_sentinel = {
    .parent = &event_timer_sentinel,
    .left = &event_timer_sentinel,
    .right = &event_timer_sentinel,
    .color = 0
};

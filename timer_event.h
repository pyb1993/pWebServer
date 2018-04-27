//
//  timer_event.h
//  pWebServer
//
//  Created by pyb on 2018/4/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef timer_event_h
#define timer_event_h

#include <stdio.h>
#include "rb_tree.h"
extern rbtree_t  event_timer_rbtree;
/* 红黑树的哨兵节点 */
extern rbtree_node_t  event_timer_sentinel;
#endif /* timer_event_h */

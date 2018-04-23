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


#if (MACOS)
    #define READ_EVENT     EVFILT_READ
    #define WRITE_EVENT    EVFILT_WRITE
#endif


typedef struct event_s event_t;
typedef void (*event_handler_pt)(event_t *ev);

// 事件结构体
struct event_s{
    void *data;// 指向connection
    event_handler_pt  handler;
    bool active;
    // timer的结构
};






#endif /* event_h */

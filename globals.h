//
//  globals.h
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef globals_h
#define globals_h
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include "stdarg.h"
#include <unistd.h>
#include <sys/types.h>

#define OK 0
#define ERROR (-1)
#define AGAIN (-2)
#define INVALID_REQUEST (-3)

typedef unsigned char uint8_t;
#define align(d, a) (((d) + (a - 1)) & ~(a - 1))
#define align_ptr(p, a) (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))
#define tolower(c)      (u_char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define memzero(buf, n)       (void) memset(buf, 0, n)

#define cacheline_size 32

#define ERR_ON(cond, msg)           \
do {                                \
if (cond) {                     \
plog("ERROR:-----%s: %d: ", \
__FILE__, __LINE__);\
plog(msg);                \
}                               \
} while (0)

#define ABORT_ON(cond, msg)          \
do {                                \
if (cond) {                     \
plog("%s: %d: ", \
__FILE__, __LINE__);\
plog(msg);                \
abort();                    \
}                               \
} while (0)


#define MASTER 0
#define WORKER 1
#define SINGLE 2

#define C_UPSTREAM 1
#define C_DIRECTSTREAM 2
#define C_IDLE 3

#define LOG_DIR "/usr/local/log/"


#define MACOS 1
#define MAX_EVENT_NUM   (65536)

#define TIMER_LAZY_DELAY 300
#define TIMER_INFINITE ((msec_t)-1)
#define UPDATE_TIME 1
#endif /* globals_h */

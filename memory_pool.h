//
//  memory_pool.h
//  pWenServer
//
//  Created by pyb on 2018/3/2.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef memory_pool_h
#define memory_pool_h

#include <stdio.h>

typedef struct header{
    struct header * next;
    unsigned usedsize;
    unsigned freesize;
} Header;

typedef struct memory_pool{
    Header * data;
    Header * memptr;
    int MEMSIZE;
} memory_pool;

u_char* pMalloc(memory_pool * pool,uint nbytes);
int pMemoryPoolInit(memory_pool* p, uint memsize);
void pFree(memory_pool * pool,void *ap);
memory_pool* createPool(uint nbytes);
void freePool(memory_pool* p);
#endif /* memory_pool_h */

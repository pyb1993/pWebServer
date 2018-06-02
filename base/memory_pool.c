//
//  memory_pool.c
//  pWenServer
//
//  Created by pyb on 2018/3/2.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "memory_pool.h"
#include "globals.h"
static const uint HEADERSIZE = sizeof(Header);

u_char* pMalloc(memory_pool * pool,uint nbytes)
{
    Header *p, *newp;
    Header *memptr = pool->memptr;
    unsigned int nunits = ((nbytes + HEADERSIZE - 1) / HEADERSIZE) + 1;
    if (memptr == NULL)
    {
        memptr = pool->data;
        memptr->usedsize = 1;
        memptr->freesize = pool->MEMSIZE - 1;
        memptr->next = memptr;//指向自己
    }
    p = memptr;
    while (p->next != memptr && (p->freesize < nunits)) { p = p->next;}
    if (p->freesize < nunits) return NULL; // no available block
    
    newp = p + p->usedsize;
    newp->usedsize = nunits;
    newp->freesize = p->freesize - nunits;
    newp->next = p->next;
    p->next = newp;
    p->freesize = 0;
    pool->memptr = newp;
    
    return (u_char*)(newp + 1);
}

//这样memptr总是指向最近一个分配的地方
void pFree(memory_pool * pool,void *ap)
{
    Header *bp, *p, *prev;
    bp = (Header*)ap - 1;
    prev = pool->memptr, p = pool->memptr->next;
    Header * memptr = pool->memptr;
    while ((p != bp) && (p != memptr)){
        prev = p;
        p = p->next;
    }
    if (p != bp) return;
    
    prev->freesize += p->usedsize + p->freesize;
    prev->next = p->next;
    pool->memptr = prev;
}

int pMemoryPoolInit(memory_pool* p, uint memsize)
{
    p->MEMSIZE = (memsize + sizeof(Header) - 1) / sizeof(Header);
    p->data = malloc(memsize + sizeof(Header));
    p->memptr = NULL;
    if (p->data == NULL) return ERROR;
    return OK;
}

memory_pool* createPool(uint bytes){
    memory_pool* p = malloc(sizeof(memory_pool));
    int ret = pMemoryPoolInit(p, bytes);
    if(ret != OK) return NULL;
    return p;
}

void freePool(memory_pool* p){
    free(p->data);
    free(p);
}

 //
//  pool.c
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "pool.h"
#include "commonUtil.h"
int poolInit(pool* p,int unit_size,int unit_num,int chunk_num){
    unit_size = max(unit_size,sizeof(chunk_slot));
    p->used = 0;
    p->unit_num = unit_num;
    p->unit_size = unit_size;
    p->free = NULL;

    int err = vectorInit(&p->chunks, chunk_num,sizeof(chunk));
    if(chunk_num == 0){
        return err;
    }
    
    for(int i = 0; i < chunk_num; ++i){
        chunk* cur_chunk = vectorAt(&p->chunks,i);
        int err = chunkInit(cur_chunk,unit_size,unit_num);
        if(err != OK){
            plog("pool init error");
            return err;
        }
        if(i > 0){
            chunk* last_chunk = vectorAt(&p->chunks,i - 1);
            chunk_slot* last_chunk_slot = (char*)(last_chunk->data) + (unit_size) * (unit_num - 1);
            last_chunk_slot->next = cur_chunk->data;
        }
    }
    
    chunk* first_chunk = vectorAt(&p->chunks,0);
    p->free = first_chunk->data;
    return OK;
}


void* poolAlloc(pool* p){
    p->used++;
    chunk_slot* new_chunk_slot;
    chunk* new_chunk;
    if(p->free == NULL){
        new_chunk = vectorPush(&p->chunks);// alloc a new chunk
        if(new_chunk == NULL) return NULL;
        if(chunkInit(new_chunk,p->unit_size,p->unit_num) != OK){
            return NULL;
        }
        new_chunk_slot = (chunk_slot*)(new_chunk->data);
        p->free = new_chunk_slot;
    }
    
    void* ret = p->free;
    p->free = ((chunk_slot*)ret)->next;
    return ret;
}


//free就指向chunk里第一个free的元素
//注意,假设现在有两个chunk c1,c2 如果freec2里面的第一个单元,那么pool->cur = c2.first -> old_cur(链接着原来可用的chunk_slot)->...->最后一个chunk中没有用完的slot
 void poolFree(pool* p, void* x) {
    if (x == NULL) {
        return;
    }
    --p->used;
    ((chunk_slot*)x)->next = p->free;
    p->free = x;
}

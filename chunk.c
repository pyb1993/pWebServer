//
//  chunk.c
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "chunk.h"
#include "commonUtil.h"

/*
 */
int chunkInit(chunk* c,int unit_size,int unit_num){
    ABORT_ON(unit_num <= 0,"error about unit num");
    c->data = malloc(unit_size * unit_num);
    if(c->data == NULL ) return ERROR;

    chunk_slot * slot = c->data;
    uint8_t* ele = (uint8_t* )c->data;// ensure ele + x will go x bytes
    for(int i = 1; i < unit_num; ++i){
        slot->next = (uint8_t*)slot + unit_size;
        slot = slot->next;
    }
    
    slot->next = NULL;
    return OK;
}

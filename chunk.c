//
//  chunk.c
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "chunk.h"
#include "globals.h"
#include "commonUtil.h"
#include <string.h>

/*
  对一个chunk进行初始化,chunk一共有unit_num的slot,每一个需要的大小是unit_size
 */
int chunkInit(chunk* c,int unit_size,int unit_num){
    ABORT_ON(unit_num <= 0,"error about unit num");
    // 这里需要多出一个void*的长度
    unit_size += sizeof(void*);
    
    // data分配内存
    c->data = malloc(unit_size * unit_num);
    memzero(c->data, unit_size * unit_num);
    if(c->data == NULL ) return ERROR;

    chunk_slot * slot = c->data;
    for(int i = 1; i < unit_num; ++i){
        slot->next = (uint8_t*)slot + unit_size;
        slot = slot->next;
    }
    
    slot->next = NULL;
    return OK;
}

/*跳过next的转变*/
void* get_data_from_chunk(chunk_slot* chunk_slot_ele){
    void* data = (uint8_t*)chunk_slot_ele + sizeof(void*);
    return data;
}

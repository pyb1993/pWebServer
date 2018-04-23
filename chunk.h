//
//  chunk.h
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef chunk_h
#define chunk_h

#include <stdio.h>
#include "globals.h"

typedef union{
    void * next;
} chunk_slot;

typedef struct chunk{
    void * data;
}chunk;


int chunkInit(chunk* c,int unit_size,int unit_num);

#endif /* chunk_h */

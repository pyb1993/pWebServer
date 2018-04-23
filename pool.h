//
//  pool.h
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef pool_h
#define pool_h

#include <stdio.h>
#include "vector.h"
#include "chunk.h"
/*
 vector chunks
 .........chunk1:
 ...........unit1(unit_size) unit2 unit3 ... unit_size
 .........chunk2
 .........
 .........chunkN
 
 
 */


/*
 这样分配的逻辑在于,chunks作为每次从内存分配的最小单元，一次分配unit_num个
 这样接下来的unit_num-1次分配都不需要再分配新的数据了
 */
typedef struct pool{
    int unit_size;// a unit of chunk occupy
    int unit_num;// a chunk occupy how many unit
    int used;// how many unit are used
    void * free;
    vector chunks;
} pool;

int poolInit(pool* p,int unit_size,int unit_num,int chunk_num);
void* poolAlloc(pool* p);
void poolFree(pool* p, void* x);

#endif /* pool_h */

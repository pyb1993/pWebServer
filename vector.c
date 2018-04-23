//
//  vector.c
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

/* vector is used as array */
#include "vector.h"
#include "commonUtil.h"
int vectorInit(vector * vec,int capacity, int unit_size)
{
    assert(unit_size > 0);
    
    if(capacity > 0){
        vec->data = (void *)malloc(capacity * unit_size);
        if(vec->data == NULL) return ERROR;
    }
    else{
        vec->data = NULL;
    }
    vec->used = 0;
    vec->capacity = capacity;
    vec->unit_size = unit_size;
    return OK;
}

int vectorResize(vector * vec,int new_capacity)
{
    if(vec->capacity >= new_capacity) return OK;
    
    new_capacity = max(vec->capacity * 1.5,new_capacity);
    vec->data = realloc(vec->data, new_capacity * vec->unit_size);
    if(vec->data == NULL) ERROR;
    
    vec->capacity = new_capacity;
    return OK;
}

void * vectorAt(vector* vec,int pos)
{
    if(pos >= vec->capacity || pos < 0) return NULL;
    return (void*)((char*)vec->data + vec->unit_size * pos);
}

void vectorPop(vector* vec)
{
    assert(vec->used > 0);
    vec->used--;
}


void* vectorBack(vector* vec){
    return vectorAt(vec,vec->used - 1);
}

void* vectorPush(vector* vec)
{
    if(vectorResize(vec, ++vec->used) == OK){
        return vectorBack(vec);
    }
    return NULL;
}


void vectorClear(vector * vec){
    vec->capacity = 0;
    vec->used = 0;
    vec->unit_size = 0;
    free(vec->data);
    vec->data = NULL;
}

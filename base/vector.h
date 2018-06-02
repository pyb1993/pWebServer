//
//  vector.h
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef vector_h
#define vector_h
#include "globals.h"
#include <stdio.h>
/*intrusive data structure, used manage memory */
typedef struct vector{
    int capacity;
    int used;
    int unit_size;
    void * data;
} vector;

int vectorInit(vector * vec,int capacity, int unit_size);
int vectorResize(vector * vec,int new_capacity);
void* vectorPush(vector* vec);
void * vectorAt(vector* vec,int pos);
void vectorPop(vector* vec);
void* vectorBack(vector* vec);

#endif /* vector_h */

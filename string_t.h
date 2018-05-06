//
//  string.h
//  pWenServer
//
//  Created by pyb on 2018/3/1.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef string_h
#define string_h
#include <stdio.h>
#include "string.h"

#define STRING(cstr) (string){cstr,sizeof(cstr) - 1}

typedef struct string{
    char * c;
    long len;
} string;

extern const string NULL_STR;
int stringEq(string* a,string* b);
void stringInit(string * str);
#endif /* string_h */

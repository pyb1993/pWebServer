//
//  string.c
//  pWenServer
//
//  Created by pyb on 2018/3/1.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "string_t.h"
const string NULL_STR = {NULL, 0};

void stringInit(string * str){
    str->c = NULL;
    str->len = 0;
}

int stringEq(string* a,string* b){
    if(a->len != b->len) {return 0;}
    return strncmp(a->c,b->c,a->len) == 0;
}



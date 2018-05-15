//
//  commonUtil.c
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "commonUtil.h"
#include "globals.h"
#include <unistd.h>
#include <math.h>

int max(int x, int y) {
    return x > y ? x : y;
}

int min(int x,int y){
    return x < y ? x : y;
}

int p_abs(int x){
    return x < 0 ? -x : x;
}

int p_ceil(float x){
    return ceil(x);
}

void plog(const char* format, ...) {
    FILE* log_file = fopen(LOG_DIR "pserver.log", "a+");
    if (log_file == NULL) {
        printf("open log failed\n");
        perror("open failed\n");
        return;
    }
    
    fprintf(log_file, "[pid: %5d]",getpid());
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fprintf(log_file, "\n");
    fclose(log_file);
}

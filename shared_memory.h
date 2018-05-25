//
//  shared_memory.h
//  pWebServer
//
//  Created by pyb on 2018/5/20.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef shared_memory_h
#define shared_memory_h

#include <stdio.h>
#include "string_t.h"

#if (NGX_SMP)
#define NGX_SMP_LOCK  "lock;"
#else
#define NGX_SMP_LOCK
#endif

#define accept_mutex_lock shmtx_try_lock(&accept_mutex)
#define accept_mutext_unlock shmtx_try_unlock(&accept_mutex)
typedef long atomic_int_t;
typedef unsigned long atomic_uint_t;
typedef volatile atomic_uint_t atomic_t;

/*共享内存的结构体定义*/
typedef struct {
    u_char      *addr;      // 共享内存首地址
    size_t      size;      // 共享内存大小
    string      name;      // 共享内存名称
} shm_t;


/*自旋锁的结构体定义*/
typedef struct {
    atomic_t*  lock;
    uint spin;
} shmtx_t;


int shm_alloc(shm_t* shared_mem);
int shmtx_create(shmtx_t *mtx, void *addr);
int shmtx_try_lock(shmtx_t *mtx);
void shmtx_try_unlock(shmtx_t *mtx);



#endif /* shared_memory_h */

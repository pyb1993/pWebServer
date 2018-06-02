//
//  shared_memory.c
//  pWebServer
//
//  Created by pyb on 2018/5/20.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "shared_memory.h"
#include "globals.h"
#include "commonUtil.h"
#include <sys/mman.h>

/*利用汇编直接定义了CAS操作*/
static inline atomic_uint_t atomic_cmp_set(atomic_t *lock, atomic_uint_t old,atomic_uint_t set)
{
    u_char  res;
    
    __asm__ volatile (
                      
                      NGX_SMP_LOCK
                      "    cmpxchgq  %3, %1;   "
                      "    sete      %0;       "
                      
                      : "=a" (res) : "m" (*lock), "a" (old), "r" (set) : "cc", "memory");
    
    return res;
    
}


/*为共享内存分配数据结构
  使用mmap来实现
 */
int shm_alloc(shm_t* shared_mem){

    /*mmap的参数解释:
        PROT_READ | PROT_WRITE 表示分配的匿名页内存又可以读又可以写
        MAP_ANON | MAP_SHARED 表明这个不是和文件关联的页面,同时也表明可以多个进程共享
     */
    shared_mem->addr = (u_char *) mmap(NULL, shared_mem->size,
                                PROT_READ | PROT_WRITE,
                                MAP_ANON | MAP_SHARED, -1, 0);
    if (shared_mem->addr == MAP_FAILED) {
        plog("err on shared memory alloc");
        return ERROR;
    }
    return OK;
}

/*释放函数,暂时不知道什么时候用到*/
void
shm_free(shm_t *shared_mem)
{
    if (munmap((void *) shared_mem->addr, shared_mem->size) == -1) {
        plog("munmap(%p, %uz) failed", shared_mem->addr, shared_mem->size);
    }
}

/*
  利用共享内存创建自旋锁
 */
int shmtx_create(shmtx_t *mtx, void *addr)
{
    // 指向共享内存中的地址
    mtx->lock = addr;
    // 如果有指定为-1，则表示关掉自旋等待，在后面代码中我们可以看到
    if(mtx->spin == (uint) -1) {
        return OK;
    }
    
    // 默认为2048
    mtx->spin = 2048;
    return OK;
}

/*共享锁的加锁机制
  这里实现的是非阻塞锁,如果加锁成功,那么就返回1,否则就返回0
  todo 这里有一个对于负载均衡的优化
  假设当前已经占了总可用connection数量的1/4
  那么我们就开始减少获取锁的概率,实现简单的负载均衡,比如说:
  超过2/8,获取锁的概率是7/8
  超过4/8 获取锁的概率是6/8,
  超过5/8,获取锁的概率是5/8
  超过6/8,获取锁的概率是4/8
  超过7/8,概率是0
 */

int shmtx_try_lock(shmtx_t *mtx)
{
    int ret = (*mtx->lock == 0 && atomic_cmp_set(mtx->lock, 0, 1));
    if(ret == 1){
        plog("try to lock success");
    }else{
        plog("try to lock fail");
    }
    return ret;
}


/*共享锁的解锁机制
  这里实现的是非阻塞机制
 */

void shmtx_try_unlock(shmtx_t *mtx)
{
    atomic_uint_t  old;
    while(1) {
        old = *mtx->lock;
        if (atomic_cmp_set(mtx->lock, old, 0)) {
            plog("try to unlock success");
            break;
        }
    }
}















//
//  pWebServer
//
//  Created by pyb on 2018/4/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef rb_tree_h
#define rb_tree_h

#include <stdio.h>

typedef uint32_t  rbtree_key_t;
typedef int32_t   rbtree_key_int_t;

#define timer_after(b,a) ((rbtree_key_int_t)((a) - (b)) < 0)
#define is_expired(key,cur_msec) timer_after(key,cur_msec)


typedef struct rbtree_node_s  rbtree_node_t;
typedef void (*rbtree_insert_pt) (rbtree_node_t *temp, rbtree_node_t *node,rbtree_node_t *s);

typedef struct rbtree_node_s{
    rbtree_key_t key;       // key的值
    rbtree_node_t *parent;  // 父节点
    rbtree_node_t *left;    //右子节点
    rbtree_node_t *right;   //左子节点
    u_char color;// 节点的颜色
} rbtree_node_t;

typedef struct rbtree_s{
    rbtree_node_t     *root;    /* 指向树的根节点 */
    rbtree_node_t     *sentinel;/* 指向树的叶子节点NIL */
    rbtree_insert_pt   insert;  /* 添加元素节点的函数指针，解决具有相同键值，但不同颜色节点的冲突问题；
                                   该函数指针决定新节点的行为是新增还是替换原始某个节点*/
} rbtree_t;

/* 给节点着色，1表示红色，0表示黑色  */
#define rbt_black(node)             ((node)->color = 0)
#define rbt_red(node)               ((node)->color = 1)
/* 判断节点的颜色 */
#define rbt_is_red(node)            ((node)->color)
#define rbt_is_black(node)          (!rbt_is_red(node))
/* 复制某个节点的颜色 */
#define rbt_copy_color(n1, n2)      (n1->color = n2->color)
/* 初始化红黑树的哨兵节点,空叶子结点必须是黑色的*/
#define rbtree_sentinel_init(node)  rbt_black(node)

/*初始化红黑树*/
#define rbtree_init(tree,s,i)\
(tree)->root = s;      \
(tree)->sentinel = s;  \
(tree)->insert = i     \

#define is_left(node)((node) == (node)->parent->left)
#define is_right(node)(!is_left(node))
#define father(node)((node)->parent)
#define grand_father(node)((node)->parent->parent)
void rbtree_insert_timer_value(rbtree_node_t *temp, rbtree_node_t *node,rbtree_node_t *sentinel);
void rbtree_insert(rbtree_t* tree,rbtree_node_t * node);
void rbtree_delete(rbtree_t* tree,rbtree_node_t* node);
void rbtree_fix_after_delete(rbtree_t* tree,rbtree_node_t* node);
rbtree_node_t* rbtree_min(rbtree_node_t *node, rbtree_node_t *sentinel);


#endif /* rb_tree_h */

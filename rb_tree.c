//
//  rb_tree.c
//  pWebServer
//
//  Created by pyb on 2018/4/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "rb_tree.h"

/*对红黑树进行左旋,以node为支点
     node
        .
         .
          y
 */
void rbtree_left_rotate(rbtree_node_t **root,rbtree_node_t * sentinel,rbtree_node_t *node){
    rbtree_node_t  *temp;
    
    temp = node->right;/* temp为node节点的右孩子 */
    node->right = temp->left;/* 设置node节点的右孩子为temp的左孩子 */
    
    if (temp->left != sentinel) {
        temp->left->parent = node;
    }
    
    temp->parent = node->parent;
    
    if (node == *root) {
        *root = temp;
        
    } else if (node == node->parent->left) {
        node->parent->left = temp;
        
    } else {
        node->parent->right = temp;
    }
    
    temp->left = node;
    node->parent = temp;
}

/*对红黑树进行右旋,以node为支点
    node
    .
   .
 y
 */
void rbtree_right_rotate(rbtree_node_t **root,rbtree_node_t* sentinel, rbtree_node_t* node){
    rbtree_node_t  *temp;
    
    temp = node->left;
    node->left = temp->right;
    
    if (temp->right != sentinel) {
        temp->right->parent = node;
    }
    
    temp->parent = node->parent;
    
    if (node == *root) {
        *root = temp;
        
    } else if (node == node->parent->right) {
        node->parent->right = temp;
        
    } else {
        node->parent->left = temp;
    }
    
    temp->right = node;
    node->parent = temp;
}

/*
 * 按照二叉查找树的顺序进行插入
   now表示开始比较的节点
   在每一次循环中,now是父节点,p是子节点,当p等于sentinel的时候结束
   为什么要用双重指针来比较,因为需要将某个节点直接赋值为node,这样可以减少一次对p是now左节点还是右节点的判断
   (*p)指向sentinel,所以现在需要修改(*p)的值,使得它指向node
 */
void rbtree_insert_value(rbtree_node_t *now, rbtree_node_t *node,
                        rbtree_node_t *sentinel)
{
    rbtree_node_t  **p;
    
    while(1) {
        
        /* 判断node节点键值与temp节点键值的大小，以决定node插入到temp节点的左子树还是右子树 */
        p = (node->key < now->key) ? &now->left : &now->right;
        
        if (*p == sentinel) {
            break;
        }
        
        now = *p;
    }
    
    /* 初始化node节点，并着色为红色 */
    *p = node;
    node->parent = now;
    node->left = sentinel;
    node->right = sentinel;
    rbt_red(node);
}

void
rbtree_insert_timer_value(rbtree_node_t *temp, rbtree_node_t *node,
                              rbtree_node_t *sentinel)
{
    rbtree_node_t  **p;
    
    for ( ;; ) {
        
        /* 判断node节点键值与temp节点键值的大小，以决定node插入到temp节点的左子树还是右子树 */
        p = (node->key < temp->key) ? &temp->left : &temp->right;
        
        if (*p == sentinel) {
            break;
        }
        
        temp = *p;
    }
    
    /* 初始化node节点，并着色为红色 */
    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    rbt_red(node);
}

/*按照二叉树的规则,将节点node插入红黑树,并且保持红黑树的性质不发生变化
  root之所以是双重指针,是因为可能需要修改root
 */
void rbtree_insert(rbtree_t* tree,rbtree_node_t * node)
{
    rbtree_node_t  **root, *temp, *sentinel;
    
    /* a binary tree insert */
    
    root = (rbtree_node_t **) &tree->root;
    sentinel = tree->sentinel;
    
    /* 若红黑树为空，则比较简单，把新节点作为根节点，
     * 并初始化该节点使其满足红黑树性质
     */
    if (*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        rbt_black(node);
        *root = node;
        
        return;
    }
    
    /* 若红黑树不为空，则按照二叉查找树的插入操作进行
     * 该操作由函数指针提供
     */
    tree->insert(*root, node, sentinel);
    
    /* re-balance tree */
    
    /* 调整红黑树，使其满足性质，
     * 其实这里只是破坏了性质4：若一个节点是红色，则孩子节点都为黑色；
     * 若破坏了性质4，则新节点 node 及其父亲节点 node->parent 都为红色；
     */
    while (node != *root && rbt_is_red(node->parent)) {
        
        /* 若node的父亲节点是其祖父节点的左孩子 */
        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;/* temp节点为node的叔叔节点 */
            
            /* case1：node的叔叔节点是红色 */
            /* 此时，node的父亲及叔叔节点都为红色；
             * 解决办法：将node的父亲及叔叔节点着色为黑色，将node祖父节点着色为红色；
             * 然后沿着祖父节点向上判断是否会破会红黑树的性质；
             */
            if (rbt_is_red(temp)) {
                rbt_black(node->parent);
                rbt_black(temp);
                rbt_red(node->parent->parent);
                node = node->parent->parent;
                
            } else {
                /* case2：node的叔叔节点是黑色且node是父亲节点的右孩子 */
                /* 则此时，以node父亲节点进行左旋转，使case2转变为case3；
                 */
                if (node == node->parent->right) {
                    node = node->parent;
                    rbtree_left_rotate(root, sentinel, node);
                }
                
                /* case3：node的叔叔节点是黑色且node是父亲节点的左孩子 */
                /* 首先，将node的父亲节点着色为黑色，祖父节点着色为红色；
                 * 然后以祖父节点进行一次右旋转；
                 */
                rbt_black(node->parent);
                rbt_red(node->parent->parent);
                rbtree_right_rotate(root, sentinel, node->parent->parent);
            }
            
        } else {/* 若node的父亲节点是其祖父节点的右孩子 */
            /* 这里跟上面的情况是对称的，就不再进行讲解了
             */
            temp = node->parent->parent->left;
            
            if (rbt_is_red(temp)) {
                rbt_black(node->parent);
                rbt_black(temp);
                rbt_red(node->parent->parent);
                node = node->parent->parent;
                
            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    rbtree_right_rotate(root, sentinel, node);
                }
                
                rbt_black(node->parent);
                rbt_red(node->parent->parent);
                rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }
    
    /* 根节点必须为黑色 */
    rbt_black(*root);

}

/* 获取红黑树键值最小的节点 */
rbtree_node_t* rbtree_min(rbtree_node_t *node, rbtree_node_t *sentinel)
{
    while (node->left != sentinel) {
        node = node->left;
    }
    return node;
}

/*删除一个节点,并且按照红黑树的方式进行调整*/
void rbtree_fix_after_delete(rbtree_t* tree,rbtree_node_t* temp){
    rbtree_node_t *w,**root,*sentinel;
    root = &tree->root;
    sentinel = tree->sentinel;
    
    
    /* 根据temp节点进行调整 ，若temp不是根节点且为黑色
       如果temp是根节点,那么直接设置为黑色,不影响任何性质
       如果temp是红色,那么也可以直接结束,因为temp实际上是(红+黑),不影响高度,将temp直接设置为黑色,不影响任何性质
     */
    while (temp != *root && rbt_is_black(temp)){
        if (is_left(temp)) {
            w = temp->parent->right;/* w为temp的兄弟节点 */
            
            /* case A：temp兄弟节点为红色 */
            /*目的: 将叔叔节点变成黑色*/
            /* 解决办法：
             * 1、改变w节点及temp父亲节点的颜色；
             * 2、对temp父亲节的做一次左旋转，此时，temp的兄弟节点是旋转之前w的某个子节点，该子节点颜色为黑色；
             * 3、此时，case A已经转换为case B、case C 或 case D；
             */
            if (rbt_is_red(w)) {
                rbt_black(w);
                rbt_red(temp->parent);
                rbtree_left_rotate(root,sentinel, temp->parent);
                w = temp->parent->right;
            }

            /* case B：temp的兄弟节点w是黑色，且w的两个子节点都是黑色 */
            /* 解决办法：
             * 1 改变w节点的颜色；
             * 2 将w和temp同时去掉一重黑色,然后将这个黑色转移到父节点上
             * 3 把temp的父亲节点作为新的temp节点；
             */
            if (rbt_is_black(w->left) && rbt_is_black(w->right)) {
                rbt_red(w);
                temp = temp->parent;
            }else{
                /* case C：temp的兄弟节点w是黑色，且w的左孩子是红色，右孩子是黑色 */
                /* 解决办法：
                 * 1、将改变w及其左孩子的颜色；
                 * 2、对w节点进行一次右旋转；
                 * 3、此时，temp新的兄弟节点w有着一个红色右孩子的黑色节点，转为case D；
                 */
                if (rbt_is_black(w->right)) {
                    rbt_black(w->left);
                    rbt_red(w);
                    rbtree_right_rotate(root,sentinel, w);
                    w = temp->parent->right;
                }
                
                /* case D：temp的兄弟节点w为黑色，且w的右孩子为红色 */
                /* 解决办法：
                 * 1、将w节点设置为temp父亲节点的颜色，temp父亲节点设置为黑色；
                 * 2、w的右孩子设置为黑色；
                 * 3、对temp的父亲节点做一次左旋转；
                 * 4、最后把根节点root设置为temp节点；
                 */
                rbt_copy_color(w, temp->parent);
                rbt_black(temp->parent);
                rbt_black(w->right);
                rbtree_left_rotate(root,sentinel, temp->parent);
                temp = *root;
            }
        }else{
        // 针对temp是父亲右孩子的情况
            w = temp->parent->left;
            
            if (rbt_is_red(w)) {
                rbt_black(w);
                rbt_red(temp->parent);
                rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }
            
            if (rbt_is_black(w->left) && rbt_is_black(w->right)) {
                rbt_red(w);
                temp = temp->parent;
                
            } else {
                if (rbt_is_black(w->left)) {
                    rbt_black(w->right);
                    rbt_red(w);
                    rbtree_left_rotate(root,sentinel, w);
                    w = temp->parent->left;
                }
                
                rbt_copy_color(w, temp->parent);
                rbt_black(temp->parent);
                rbt_black(w->left);
                rbtree_right_rotate(root,sentinel, temp->parent);
                temp = *root;
            }
        }
    }
    rbt_black(temp);
}




/* 删除节点 */
void
rbtree_delete( rbtree_t *tree,rbtree_node_t *node)
{
    uint           red;
    rbtree_node_t  **root, *sentinel, *subst, *temp, *w;
    
    /* a binary tree delete */
    
    root = (rbtree_node_t **) &tree->root;
    sentinel = tree->sentinel;
    
    /* 下面是获取temp节点值，temp保存的节点是准备替换节点node ；
     * subst是保存要被替换的节点的后继节点；
     */
    
    /* case1：若node节点没有左孩子（这里包含了存在或不存在右孩子的情况）*/
    if (node->left == sentinel) {
        temp = node->right;
        subst = node;
        
    } else if (node->right == sentinel) {/* case2：node节点存在左孩子，但是不存在右孩子 */
        temp = node->left;
        subst = node;
        
    } else {/* case3：node节点既有左孩子，又有右孩子 */
        subst = rbtree_min(node->right, sentinel);/* 获取node节点的后续节点 */
        
        if (subst->left != sentinel) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }
    
    /* 若被替换的节点subst是根节点，则temp直接替换subst称为根节点 */
    if (subst == *root) {
        *root = temp;
        rbt_black(temp);
        
        /* DEBUG stuff */
        node->left = sentinel;
        node->right = sentinel;
        node->parent = sentinel;
        node->key = 0;
        
        return;
    }
    
    /* red记录subst节点的颜色 */
    red = rbt_is_red(subst);
    
    /* temp节点替换subst 节点 */
    if (subst == subst->parent->left) {
        subst->parent->left = temp;
        
    } else {
        subst->parent->right = temp;
    }
    
    /* 根据subst是否为node节点进行处理 */
    if (subst == node) {
        temp->parent = subst->parent;
        
    } else {
        
        if (subst->parent == node) {
            temp->parent = subst;
            
        } else {
            temp->parent = subst->parent;
        }
        
        /* 复制node节点属性 */
        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        rbt_copy_color(subst, node);
        
        if (node == *root) {
            *root = subst;
            
        } else {
            if (node == node->parent->left) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }
        
        if (subst->left != sentinel) {
            subst->left->parent = subst;
        }
        
        if (subst->right != sentinel) {
            subst->right->parent = subst;
        }
    }
    
    /* DEBUG stuff */
    node->left = sentinel;
    node->right = sentinel;
    node->parent = sentinel;
    node->key = 0;
    
    if (red) {
        return;
    }
    
    /* 下面开始是调整红黑树的性质 */
    /* a delete fixup */
    
    /* 根据temp节点进行处理 ，若temp不是根节点且为黑色 */
    while (temp != *root && rbt_is_black(temp)) {
        
        /* 若temp是其父亲节点的左孩子 */
        if (temp == temp->parent->left) {
            w = temp->parent->right;/* w为temp的兄弟节点 */
            
            /* case A：temp兄弟节点为红色 */
            /* 解决办法：
             * 1、改变w节点及temp父亲节点的颜色；
             * 2、对temp父亲节的做一次左旋转，此时，temp的兄弟节点是旋转之前w的某个子节点，该子节点颜色为黑色；
             * 3、此时，case A已经转换为case B、case C 或 case D；
             */
            if (rbt_is_red(w)) {
                rbt_black(w);
                rbt_red(temp->parent);
                rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }
            
            /* case B：temp的兄弟节点w是黑色，且w的两个子节点都是黑色 */
            /* 解决办法：
             * 1、改变w节点的颜色；
             * 2、把temp的父亲节点作为新的temp节点；
             */
            if (rbt_is_black(w->left) && rbt_is_black(w->right)) {
                rbt_red(w);
                temp = temp->parent;
                
            } else {/* case C：temp的兄弟节点是黑色，且w的左孩子是红色，右孩子是黑色 */
                /* 解决办法：
                 * 1、将改变w及其左孩子的颜色；
                 * 2、对w节点进行一次右旋转；
                 * 3、此时，temp新的兄弟节点w有着一个红色右孩子的黑色节点，转为case D；
                 */
                if (rbt_is_black(w->right)) {
                    rbt_black(w->left);
                    rbt_red(w);
                    rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }
                
                /* case D：temp的兄弟节点w为黑色，且w的右孩子为红色 */
                /* 解决办法：
                 * 1、将w节点设置为temp父亲节点的颜色，temp父亲节点设置为黑色；
                 * 2、w的右孩子设置为黑色；
                 * 3、对temp的父亲节点做一次左旋转；
                 * 4、最后把根节点root设置为temp节点；*/
                rbt_copy_color(w, temp->parent);
                rbt_black(temp->parent);
                rbt_black(w->right);
                rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
            
        } else {/* 这里针对的是temp节点为其父亲节点的左孩子的情况 */
            w = temp->parent->left;
            
            if (rbt_is_red(w)) {
                rbt_black(w);
                rbt_red(temp->parent);
                rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }
            
            if (rbt_is_black(w->left) && rbt_is_black(w->right)) {
                rbt_red(w);
                temp = temp->parent;
                
            } else {
                if (rbt_is_black(w->left)) {
                    rbt_black(w->right);
                    rbt_red(w);
                    rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }
                
                rbt_copy_color(w, temp->parent);
                rbt_black(temp->parent);
                rbt_black(w->left);
                rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }
    
    rbt_black(temp);
}






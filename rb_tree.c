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
void rbtree_left_rotate(rbtree_node_t **root,rbtree_node_t *node,rbtree_node_t * sentinel){
    rbtree_node_t* y = node->right;

    // 顺序非常重要
    node->right = y->left;
    if(y->left != sentinel){
        // 注意需要判断,y->left是不是空节点
        y->left->parent = node;
    }
    
    y->parent = node->parent;
    if(node->parent == sentinel){
    //  node是根节点了
        *root = node;
    }
    else if(node->parent->left == node){
        // node是左子节点
        node->parent->left = y;
    }else if(node->parent->right == node){
        // node是右子节点
        node->parent->right = y;
    }
    
    y->left = node;
    node->parent = y;
}

/*对红黑树进行右旋,以node为支点
    node
    .
   .
 y
 */
void rbtree_right_rotate(rbtree_node_t **root, rbtree_node_t* node,rbtree_node_t* sentinel){
    rbtree_node_t  *y;
    
    y = node->left;
    node->left = y->right;
    
    if (y->right != sentinel) {
        y->right->parent = node;
    }
    
    y->parent = node->parent;
    
    if (node == *root) {
        *root = y;
        
    } else if (node == node->parent->right) {
        node->parent->right = y;
        
    } else {
        node->parent->left = y;
    }
    
    y->right = node;
    node->parent = y;
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
        
        /*
         * Timer values
         * 1) are spread in small range, usually several minutes,
         * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
         * The comparison takes into account that overflow.
         */
        
        /*  node->key < temp->key */
        
        p = timer_after(temp->key,node->key) ? &temp->left : &temp->right;
        
        if (*p == sentinel) {
            break;
        }
        
        temp = *p;
    }
    
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
    
    /* 若红黑树为空,把新节点作为根节点，
     */
    if (*root == sentinel) {
        node->parent = sentinel;
        node->left = sentinel;
        node->right = sentinel;
        rbt_black(node);
        *root = node;
        return;
    }
    
    tree->insert(*root,node,sentinel);
    
    /* 调整红黑树，使其满足性质,一直循环的条件是node不在根节点,且父节点是红色,否则不需要处理
     */
    while(node != *root && rbt_is_red(node->parent)){
        if(is_left(node->parent)){
            /*该父节点是祖父的左孩子,叔叔是右孩子*/
            rbtree_node_t* temp = grand_father(node)->right;//叔叔节点
            
            if(rbt_is_red(temp)){
                /*case 1
                 *将父节点变黑,祖父节点变红,叔叔节点变黑
                 *可以证明,所有性质不变
                 */
                rbt_black(father(node));
                rbt_black(temp);
                rbt_red(grand_father(node));
                node = grand_father(node);
            }
        
            else{
                if(is_right(node)){
                    /*
                     case 2 叔叔是黑色,且node是右节点
                     以node父亲为中心进行左旋,使得情况变成case3
                     在case3中,原来的parent现在其实是子节点,要先解决子节点,再解决父节点
                     */
                    node = node->parent;
                    rbtree_left_rotate(root, node, sentinel);
                }
                
                /*case3 叔叔是黑色,且node是左节点
                  将父节点设置为黑色,祖父节点设置为红色此时只有叔叔节点上的性质5不满足
                  对祖父节点进行右旋,使得黑色高度增加的父节点变成当前的顶部,容易证明变换后的节点是满足条件的
                 */
                rbt_black(father(node));
                rbt_red(grand_father(node));
                rbtree_right_rotate(root, grand_father(node), sentinel);
                
            }
        }
        else{
            /*父节点是祖父的右儿子*/
            temp = grand_father(node)->left;
            
            if (rbt_is_red(temp)) {
                rbt_black(node->parent);
                rbt_black(temp);
                rbt_red(grand_father(node));
                node = grand_father(node);
            } else {
                if (is_left(node)) {
                    node = node->parent;
                    rbtree_right_rotate(root, node, sentinel);
                }
                
                rbt_black(node->parent);
                rbt_red(node->parent->parent);
                rbtree_left_rotate(root, grand_father(node),sentinel);
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
    rbtree_node_t* w,**root,*sentinel;
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
                rbtree_left_rotate(root, temp->parent,sentinel);
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
                    rbtree_right_rotate(root, w,sentinel);
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
                rbtree_left_rotate(root, temp->parent, sentinel);
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
                    rbtree_left_rotate(root, w,sentinel);
                    w = temp->parent->left;
                }
                
                rbt_copy_color(w, temp->parent);
                rbt_black(temp->parent);
                rbt_black(w->left);
                rbtree_right_rotate(root, temp->parent,sentinel);
                temp = *root;
            }
        }
    }
    rbt_black(temp);
}

/*
 删除一个node节点,按照二叉树的删除方式进行
 */
void rbtree_delete(rbtree_t* tree,rbtree_node_t* node)
{
    rbtree_node_t  **root, *sentinel, *subst, *temp, *w;
    root = (rbtree_node_t **) &tree->root;
    sentinel = tree->sentinel;
    
    while(1){
        if(node->left == sentinel){
            temp = node->right;
            subst = node;
        }
        else if (node->right == sentinel){
            temp = node->left;
            subst = node;
        }
        else{
            subst = rbtree_min(node->right, sentinel);/* 获取node节点的后续节点 */
            if (subst->left != sentinel) {
                temp = subst->left;
            } else {
                temp = subst->right;
            }
        }

        /* 若被替换的节点是根节点，则temp直接替换该节点
           这种情况一定是node节点只有左节点或者右节点,否则subst不会是根节点
         */
        if (subst == *root) {
            *root = temp;
            rbt_black(temp);
            return;
        }

        if(subst == node){
            /* case1,2,某个节点为空*/
            if (subst == subst->parent->left) {subst->parent->left = temp;}
            else {subst->parent->right = temp;}
            temp->parent = subst->parent;
            break;
        }else{
            /*case3,复制后继节点,然后递归的删除subst*/
            node->key = subst->key;
            node = subst;
            continue;
        }
    }
    
    if(rbt_is_red(node)){
        return;
    }
    
    //接下来调整性质
    #ifndef DEBUG_BIN_DELETE
    rbtree_fix_after_delete(tree, temp);
    #endif
    
}






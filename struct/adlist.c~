/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */

//创建链表初始化
list *listCreate(void)
{
    struct list *list;      //定义一个指向链表的指针
 
    if ((list = zmalloc(sizeof(*list))) == NULL)    //判断是否申请内存成功
        return NULL;    
    list->head = list->tail = NULL;         //头指针，尾指针初始化为NULL
    list->len = 0;                      //链表长度0
    list->dup = NULL;                   //dup函数指针NULL
    list->free = NULL;                  //free函数指针NULL
    list->match = NULL;                 //match函数指针NULL
    return list;                        //返回链表指针
}

/* Free the whole list.
 *
 * This function can't fail. */

//链表释放资源
void listRelease(list *list)
{
    unsigned long len;
    listNode *current, *next;
 
    current = list->head;   //指向头指针
    len = list->len;        //len长度
    while(len--) {
        next = current->next;
        if (list->free)     //如果list的free不空，即已经给予函数指针赋值
            list->free(current->value);     //则回收当前节点的value指针资源
        zfree(current);     //回收当前节点指针
        current = next;     //指向下一个节点
    }
    zfree(list);        //释放链表指针
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */

//从头指针添加节点
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;         //定义一个节点

    if ((node = zmalloc(sizeof(*node))) == NULL)    //分配空间
        return NULL;
    node->value = value;                    //赋值
    if (list->len == 0) {                   //一个节点都没有的时候，进行的添加
        list->head = list->tail = node;     //那么该节点，同时被head、tail同时指向
        node->prev = node->next = NULL;     //该节点的前向指针和后向指针都为NULL
    } else {                                 //如果不是添加第一个节点的情况下
        /*
         *在链表的头添加节点，首先先把node的前向指针指向空
         *首节点的前向指针指向node，node的后向指针指向首节点
         *head指针指向node指针
         *完成节点的添加
         * */
        node->prev = NULL;                  
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    list->len++;
    return list;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */

//从尾指针添加节点
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;     //同上
    } else {
        /*
         *node的前向指针指向tail节点
         *node的后向指针指向NULL
         *tail节点的后向指针指向node节点
         *tail指针指向node节点
         */
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    list->len++;
    return list;
}


//从某位置上插入节点
//after=1时，表示是在old_node后面添加新node
//after!=1时，表示是在old_node前面添加新node
//bug：old的后向指针好像并没有指向node节点
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (after) {
        //node的前向指指向old节点，node的后向指针指向old节点的后向指针
        node->prev = old_node;
        node->next = old_node->next;
        //如果此前old指针是尾节点，则添加后的node是尾节点，尾指针指向node
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {
        //node的前向指针指向old的前向指针指向的节点
        //node的后向指针指向old节点
        node->next = old_node;
        node->prev = old_node->prev;
        //如果此前old节点是头节点，则此时的node节点变成头节点，头指针指向node
        if (list->head == old_node) {
            list->head = node;
        }
    }
    //如果node不是头尾节点，那么需要把前向后向指针指向node
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    if (node->next != NULL) {
        node->next->prev = node;
    }
    list->len++;
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
//删除某个节点
void listDelNode(list *list, listNode *node)
{
    //node前向指针不空说明不是头节点
    //则可以进行node的前节点的next指向node后节点
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;
    //node的后向指针非空，说明不是尾节点
    //则可以进行node的后节点的prev指向node的前节点
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
    //如果list的free函数指针非空，则进行free掉value
    if (list->free) list->free(node->value);
    zfree(node);
    list->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
//通过某链表上的某个方向的迭代器
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;
    //申请空间，如果失败则返回NULL
    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    if (direction == AL_START_HEAD)
        //方向如果从HEAD开始，则迭代器next指向list的head
        iter->next = list->head;
    else
        //否则则迭代器next指向list的tail
        iter->next = list->tail;
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
//回收迭代器
void listReleaseIterator(listIter *iter) {
    //回收指针
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
//重置迭代器，方向是HEAD
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

//重置迭代器，方向是TAIL
void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
//取迭代器所指向的链表节点的下一个节点
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

    if (current != NULL) {
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    //迭代器指针总比current前进一位
    //返回的是当前的iter迭代器指向的节点
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
//复制链表
list *listDup(list *orig)
{
    list *copy;             //指向一个链表的指针
    listIter *iter;         //指向一个迭代器的指针
    listNode *node;         //指向一个节点的指针

    if ((copy = listCreate()) == NULL)      //创建失败则返回NULL
        return NULL;
    copy->dup = orig->dup;          //函数指针赋值
    copy->free = orig->free;        
    copy->match = orig->match;      
    iter = listGetIterator(orig, AL_START_HEAD);        //创建指向orig的迭代器
    while((node = listNext(iter)) != NULL) {    //迭代
        void *value;                //void*的值

        if (copy->dup) {            //dup非空
            value = copy->dup(node->value);//dup复制值给value
            if (value == NULL) {            //value空的话，则可以释放资源结束了
                listRelease(copy);
                listReleaseIterator(iter);
                return NULL;
            }
        } else      //dup空的话，直接赋值
            value = node->value;
        //添加在尾节点后
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            listReleaseIterator(iter);
            return NULL;
        }
    }
    //释放迭代器资源
    listReleaseIterator(iter);
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
//链表查找，根据某个值key找到某个对应的节点
listNode *listSearchKey(list *list, void *key)
{
    listIter *iter;         //定义一个迭代器指针
    listNode *node;         //定义一个节点指针

    iter = listGetIterator(list, AL_START_HEAD);//初始化迭代器
    while((node = listNext(iter)) != NULL) {        //进行迭代
        if (list->match) {                          //如果配置了match函数，则使用match函数进行对比
            if (list->match(node->value, key)) {
                listReleaseIterator(iter);
                return node;
            }
        } else {                                //否则就使用直接相比
            if (key == node->value) {
                listReleaseIterator(iter);
                return node;
            }
        }
    }
    listReleaseIterator(iter);          //释放资源
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
//链表序号查找，根据index标号找到对应的标号的节点
listNode *listIndex(list *list, long index) {
    listNode *n;            //节点指针

    //标号是从0开始计数的，从0开始就是head开始往后
    //标号是tail开始的话，从尾节点开始的标号是-1，,2，...,-len
    if (index < 0) {       //负数
        index = (-index)-1;     //把index变成计数值的一些变换
        n = list->tail;
        while(index-- && n) n = n->prev;
    } else {
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}

/* Rotate the list removing the tail node and inserting it to the head. */
void listRotate(list *list) {
    listNode *tail = list->tail;        //节点指针指向链表的尾指针所指

    if (listLength(list) <= 1) return;  //list长度小于等于1则直接返回不用旋转

    /* Detach current tail */
    list->tail = tail->prev;            //截断尾节点，脱离开链表，尾指针前移一位
    list->tail->next = NULL;
    /* Move it as head */
    list->head->prev = tail;            //把截断的节点搭在头节点前，头指针补充上
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}

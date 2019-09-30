/* adlist.h - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structures used currently. */

//节点，有一个指向前，一个指向后的指针
//还有一个是一个void×的指针，可以指向任何数据类型，指针在32位机器中占4字节，在64位机器中占8字节
typedef struct listNode {
    struct listNode *prev;
    struct listNode *next;
    void *value;
} listNode;

//自己实现的迭代器，该迭代器指向一个list节点
//direction啥用？？迭代器的方向？
typedef struct listIter {
    listNode *next;
    int direction;
} listIter;

//封装成一个链表
//该链表的成分：
//链表头节点，尾节点
//dup函数指针，输入输出都是可以任定的数据类型
//free函数指针，同上
//
typedef struct list {
    listNode *head;
    listNode *tail;
    /*
     *下面三个方法为所有节点的公用方法，分别在相应情况下回调使用
     * */
    //复制函数指针
    void *(*dup)(void *ptr);
    //释放函数指针
    void (*free)(void *ptr);
    //匹配函数指针
    int (*match)(void *ptr, void *key);
    //列表长度
    unsigned long len;
} list;

/* Functions implemented as macros */
/*使用宏来方便表示获取链表的一些基本数据信息
 *
 * */
#define listLength(l) ((l)->len)//链表的长度
#define listFirst(l) ((l)->head)//链表的头节点
#define listLast(l) ((l)->tail)//链表的尾节点
#define listPrevNode(n) ((n)->prev)//链表节点的前指向
#define listNextNode(n) ((n)->next)//链表节点的后指向
#define listNodeValue(n) ((n)->value)//节点的值

#define listSetDupMethod(l,m) ((l)->dup = (m))//设置该链表的dup函数
#define listSetFreeMethod(l,m) ((l)->free = (m))//设置该链表的free函数
#define listSetMatchMethod(l,m) ((l)->match = (m))//设置该链表的匹配函数

#define listGetDupMethod(l) ((l)->dup)//调用该链表的dup函数
#define listGetFree(l) ((l)->free)//调用该链表的free函数
#define listGetMatchMethod(l) ((l)->match)//调用该链表的匹配函数

/* Prototypes */
list *listCreate(void);     //链表创建初始化
void listRelease(list *list);       //链表释放回收资源
list *listAddNodeHead(list *list, void *value);         //从头指针添加节点
list *listAddNodeTail(list *list, void *value);         //从尾指针添加节点
list *listInsertNode(list *list, listNode *old_node, void *value, int after);//从某位置上插入节点
void listDelNode(list *list, listNode *node);           //删除某个节点
listIter *listGetIterator(list *list, int direction);       //获取某链表上某个方向的迭代器
listNode *listNext(listIter *iter);         //获取迭代器上的下一个节点
void listReleaseIterator(listIter *iter);       //释放迭代器
list *listDup(list *orig);          //复制链表
listNode *listSearchKey(list *list, void *key);     //链表查找，根据某个值key找到某个对应的节点
listNode *listIndex(list *list, long index);        //链表序号查找，根据index标号找到对应的标号的节点
void listRewind(list *list, listIter *li);          //重置迭代器的方向从head开始
void listRewindTail(list *list, listIter *li);      //重置迭代器的方向从tail开始
void listRotate(list *list);                    //旋转链表

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* __ADLIST_H__ */

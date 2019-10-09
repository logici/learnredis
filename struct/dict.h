/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
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

/*dict  >  dictht[2]	> **dictEntry	> key  union(v)  *next
 *	   *dictType	> function point
 *	   *privdata
 *	   rehashindex
 *	   interator
 */

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0       //ok成功
#define DICT_ERR 1      //err错误

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)      //把没有使用的参数强制转换为void类型，这样就不会有警告了

//字典集合
typedef struct dictEntry {
    void *key;          //key键值
    union {             //联合体，以下所有的变量共用一个地址，取最大的地址
        void *val;      //value值
        uint64_t u64;   //无符号x64的整型变量
        int64_t s64;    //有符号x64的整型变量
        double d;       //double双精度浮点数
    } v;
    struct dictEntry *next;//指向dictEntry的指针
} dictEntry;

//字典类型
//封装了一些字典操作的函数
typedef struct dictType {
    unsigned int (*hashFunction)(const void *key);      //函数指针，哈希函数，根据key来计算value
    void *(*keyDup)(void *privdata, const void *key);   //复制key的方法？
    void *(*valDup)(void *privdata, const void *obj);   //复制val的方法？
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);  //key值比较方法
    void (*keyDestructor)(void *privdata, void *key);       //key的析构函数
    void (*valDestructor)(void *privdata, void *obj);       //val的析构函数
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
//哈希表结构体
typedef struct dictht {
    dictEntry **table;  //？？字典实体
    unsigned long size;     //表格可容纳字典数量
    unsigned long sizemask; 
    unsigned long used;     //正在被使用的字典数量
} dictht;

//字典主操作类
typedef struct dict {
    dictType *type;	//字典类型
    void *privdata;	//私有数据指针
    dictht ht[2];	//字典哈希表，共2张，一张旧的，一张新的
    //重定位哈希时的下标
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
    //当前正在运行的迭代器的数量
    int iterators; /* number of iterators currently running */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
/*当safe=1时，该迭代器是一个安全迭代器，它表明你可以调用dictAdd，dictFind,
 *和其他的在迭代的时候违背迭代的一些操作
 *safe=0时，时一个非安全的迭代器，只能使用dictNext函数进行操作在迭代时
 */
//字典迭代器
typedef struct dictIterator {
    dict *d;		//所管理的字典集			
    long index;		//什么的？？？下标
    int table, safe;	//table时表示的是旧表还是新表，safe表示是安全与否。什么？？？概念？？？
    dictEntry *entry, *nextEntry;	//字典实体
    /* unsafe iterator fingerprint for misuse detection. */
    //指纹？？标记？？。目的是避免非安全的迭代器滥用？？？
    long long fingerprint;
} dictIterator;

//字典扫描方法，目的？？输入输出。。怎么用？？用在哪？？
typedef void (dictScanFunction)(void *privdata, const dictEntry *de);

/* This is the initial size of every hash table */
//初始化哈希表的数目
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
/*字典释放val函数时候调用，如果dict中的dictType定义了这个函数指针，则执行它*/
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

/*使用字典主操作类里边的字典操作函数类型中的复制函数复制字典
 * 主操作类的privdata变量，到字典集合当中??
 * 好像不对，是把_val复制到privdata和.val当中。。。*/
#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        entry->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        entry->v.val = (_val_); \
} while(0)

/*有符号整型的赋值
 * */
#define dictSetSignedIntegerVal(entry, _val_) \
    do { entry->v.s64 = _val_; } while(0)

/**
 *无符号整型的赋值
 */
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { entry->v.u64 = _val_; } while(0)

/**
 * double类型的赋值
 */
#define dictSetDoubleVal(entry, _val_) \
    do { entry->v.d = _val_; } while(0)

/*
 *释放键值，如果那个释放函数指针有值的话，则执行
 */
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

/*
 * 键值赋值，同上边的值的复制valDup
 */
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        entry->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        entry->key = (_key_); \
} while(0)

/*
 * 键值比较。dict->type->keyCompare指针已经赋值就使用它来比较
 * 否则直接进行比较，key1 == key2
 */
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

//调用hashFunction函数，功能：计算出哈希值????
#define dictHashKey(d, key) (d)->type->hashFunction(key)
//返回dictEntry的key值
#define dictGetKey(he) ((he)->key)
//返回dictEntry的v联合体中的val值
#define dictGetVal(he) ((he)->v.val)
//返回回dictEntry的v联合体中的有符号整型值
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
////返回回dictEntry的v联合体中的无符号整型值
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
//返回回dictEntry的v联合体中的double类型值
#define dictGetDoubleVal(he) ((he)->v.d)
//获取dict中dictht总的表的大小
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
//获取dict中dictht的总的已被使用大小
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
//获取dict有无被重hash
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* API */
//dict字典创建初始化
dict *dictCreate(dictType *type, void *privDataPtr);
//dict字典扩增函数
int dictExpand(dict *d, unsigned long size);
//dict字典添加key-value对
int dictAdd(dict *d, void *key, void *val);
//为dict字典添加只有key的dictEntry
dictEntry *dictAddRaw(dict *d, void *key);
//替换dict中的一个key-value对
int dictReplace(dict *d, void *key, void *val);
//替换dict中的一个字典集，只提供key
dictEntry *dictReplaceRaw(dict *d, void *key);
//根据key删除dict中的dictEntry
int dictDelete(dict *d, const void *key);
//字典集删除无、不调用free的方法
int dictDeleteNoFree(dict *d, const void *key);
//释放整个dict
void dictRelease(dict *d);
//根据key寻找dict中的dictEntry
dictEntry * dictFind(dict *d, const void *key);
//根据key值来寻找val值
void *dictFetchValue(dict *d, const void *key);
//重新计算dict的长度
int dictResize(dict *d);
//获取字典dict迭代器
dictIterator *dictGetIterator(dict *d);
//获取字典dict安全迭代器
dictIterator *dictGetSafeIterator(dict *d);
//根据迭代器获取下一个字典集dictEntry
dictEntry *dictNext(dictIterator *iter);
//释放字典迭代器
void dictReleaseIterator(dictIterator *iter);
//随机获取一个字典集dictEntry
dictEntry *dictGetRandomKey(dict *d);
//打印当前字典dict的状态
void dictPrintStats(dict *d);
//输入key值和长度可以计算出索引值（使用哈希函数）
unsigned int dictGenHashFunction(const void *key, int len);
//这里是一个比较简单的哈希函数
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len);
//清空dict，参数有一个是回调函数
void dictEmpty(dict *d, void(callback)(void*));
//使能调整方法
void dictEnableResize(void);
//关闭调整方法
void dictDisableResize(void);
//hash重定位，主要从旧表映射到新表
int dictRehash(dict *d, int n);
//在给定的时间内循环执行哈希重定位
int dictRehashMilliseconds(dict *d, int ms);
//设置哈希方法的种子
void dictSetHashFunctionSeed(unsigned int initval);
//获取哈希方法的种子
unsigned int dictGetHashFunctionSeed(void);
//字典扫描方法
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata);

/* Hash table types */
//三个实例化的字典操作函数类型
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */

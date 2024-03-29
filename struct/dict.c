/* Hash Tables Implementation.
 *
 * This file implements in memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto resize if needed
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

#include "fmacros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/time.h>
#include <ctype.h>

#include "dict.h"
#include "zmalloc.h"
#include "redisassert.h"

/* Using dictEnableResize() / dictDisableResize() we make possible to
 * enable/disable resizing of the hash table as needed. This is very important
 * for Redis, as we use copy-on-write and don't want to move too much memory
 * around when there is a child performing saving operations.
 *
 * 通过dictEnableResize()和dictDisableResize()函数，我们可以手动进行运行或者
 * 禁止rehash操作。这样可以更好的配合copy-on-write机制的使用。
 * Note that even when dict_can_resize is set to 0, not all resizes are
 * prevented: a hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > dict_force_resize_ratio. 
 * 如果已经设置disable，关闭了rehash，但是如果hash表的负载因子大于
 * dict_force_resize_ratio,则还是会强制rehash。
 * */
//指示字典是否使用rehash标志
static int dict_can_resize = 1;
//负载因子的最大值
static unsigned int dict_force_resize_ratio = 5;

/* -------------------------- private prototypes ---------------------------- */

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static int _dictKeyIndex(dict *ht, const void *key);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/* -------------------------- hash functions -------------------------------- */

/* Thomas Wang's 32 bit Mix Function */
//哈希函数初始化
//输入无符号整型，输出无符号整型
unsigned int dictIntHashFunction(unsigned int key)
{
    key += ~(key << 15);
    key ^=  (key >> 10);
    key +=  (key << 3);
    key ^=  (key >> 6);
    key += ~(key << 11);
    key ^=  (key >> 16);
    return key;
}

//哈希种子
static uint32_t dict_hash_function_seed = 5381;

void dictSetHashFunctionSeed(uint32_t seed) {
    dict_hash_function_seed = seed;
}

uint32_t dictGetHashFunctionSeed(void) {
    return dict_hash_function_seed;
}

/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
//哈希函数的计算
//输入，void* 键值   len长度
//返回无符号整型
unsigned int dictGenHashFunction(const void *key, int len) {
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    uint32_t seed = dict_hash_function_seed;
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    uint32_t h = seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;

    while(len >= 4) {
        uint32_t k = *(uint32_t*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch(len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int)h;
}

/* And a case insensitive hash function (based on djb hash) */
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len) {
    unsigned int hash = (unsigned int)dict_hash_function_seed;

    while (len--)
        hash = ((hash << 5) + hash) + (tolower(*buf++)); /* hash * 33 + c */
    return hash;
}

/* ----------------------------- API implementation ------------------------- */

/* Reset a hash table already initialized with ht_init().
 * NOTE: This function should only be called by ht_destroy(). */
/*ht：哈希表
 *重置一个已经被初始化后的哈希表
 *这个函数只能被调用由ht_destroy()函数
 */
static void _dictReset(dictht *ht)
{
	//哈希实例归为NULL
    ht->table = NULL;
    //属性都置为0
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/* Create a new hash table */
/*type:哈希操作集
 *privDataPtr：
 *返回一个字典指针
 *创建一个字典
 */
dict *dictCreate(dictType *type,
        void *privDataPtr)
{
	//分配空间
    dict *d = zmalloc(sizeof(*d));
	//初始化
    _dictInit(d,type,privDataPtr);
    return d;
}

/* Initialize the hash table */
/*d:字典指针
 *type：字典操作集
 *privDataPtr：
 *返回创建成功标志
 *初始化字典
 *
 */
int _dictInit(dict *d, dictType *type,
        void *privDataPtr)
{
	//重置哈希表0 1
    _dictReset(&d->ht[0]);
    _dictReset(&d->ht[1]);
    //初始化赋值
    d->type = type;
    d->privdata = privDataPtr;
    d->rehashidx = -1;
    d->iterators = 0;
    return DICT_OK;
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USED/BUCKETS ratio near to <= 1 */
/*d：字典指针
 *返回 成功 或者 err
 *底层使用expand函数
 *
 */
int dictResize(dict *d)
{
    int minimal;

    //如果dict_can_resize不置1，或者rehashidx不为-1时，
    //进入if函数，返回err，不能重置大小
    if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;

    minimal = d->ht[0].used;
    //DICT_HT_INITIAL_SIZE表示初始化哈希表的数
    //目，该宏为4
    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE;
    return dictExpand(d, minimal);
}

/* Expand or create the hash table */
/*d：字典
 *size：大小
 *返回
 *扩展hash或者创建hash表，如果ht[0]为空，则是一次创建
 *
 */
int dictExpand(dict *d, unsigned long size)
{

    dictht n; /* the new hash table */
    //计算rehash的hash表的大小
    //计算方式：第一个大于size（ht[0]已使用的长度）的2的n次幂
    unsigned long realsize = _dictNextPower(size);

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */

    //rehashidx!=-1
    // 或者 size不能小于ht[0]已使用的值？？？为什么不能小？
    if (dictIsRehashing(d) || d->ht[0].used > size)
        return DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
    //初始化新的hash表
    n.size = realsize;
    n.sizemask = realsize-1;
    n.table = zcalloc(realsize*sizeof(dictEntry*));//分配指针数组的空间
    n.used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys. */
    //如果ht[0]为空，则说明这是一次创建并初始化字典
    if (d->ht[0].table == NULL) {
        d->ht[0] = n;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
    //完成ht[1]的初始化
    //把rehashidx置0，方便后续的迁移工作
    d->ht[1] = n;
    d->rehashidx = 0;
    return DICT_OK;
}

/* Performs N steps of incremental rehashing. Returns 1 if there are still
 * keys to move from the old to the new hash table, otherwise 0 is returned.
 * Note that a rehashing step consists in moving a bucket (that may have more
 * than one key as we use chaining) from the old to the new hash table. */
/*执行N步渐进式rehash
 *返回1表示仍有key需要从ht[0]迁移到ht[1]
 *返回0则表示已经迁移完成
 *注意，每步rehash都是以一个哈希表索引（桶）作为单位
 *一个桶可以有多个节点
 *被rehash的桶里的所有节点都被迁移到新的表里
 *
 *d:字典
 *n：
 *
 */
int dictRehash(dict *d, int n) {
    //当rehashidx！=-1时，跳出if
    if (!dictIsRehashing(d)) return 0;

    while(n--) {
        dictEntry *de, *nextde;

        /* Check if we already rehashed the whole table... */
        //已经迁移完毕，所需做的后续步骤
        if (d->ht[0].used == 0) {
            zfree(d->ht[0].table);
            d->ht[0] = d->ht[1];
            _dictReset(&d->ht[1]);
            d->rehashidx = -1;
            return 0;
        }

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
        //rehashidx不能大于size
        //在rehash模式下，reshaidx作为正在迁移的索引值
        assert(d->ht[0].size > (unsigned long)d->rehashidx);
        //寻找到非空的节点进行赋值
        while(d->ht[0].table[d->rehashidx] == NULL) d->rehashidx++;
        de = d->ht[0].table[d->rehashidx];
        /* Move all the keys in this bucket from the old to the new hash HT */
        //迁移该key下的所有节点
        //因为使用的是链地址法解决hash冲突问题
        //所以一个key上回有多个节点
        while(de) {
            unsigned int h;

            nextde = de->next;
            /* Get the index in the new hash table */
            //重新计算hash函数
            //&上掩码得出新表的排位
            //好像它这个没有做链表的处理？
            //如果重新计算出来的h有相等的情况出现
            //也就是冲突，他没有链接的处理
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
            d->ht[0].used--;
            d->ht[1].used++;
            de = nextde;
        }
        //旧表迁移走的置空
        d->ht[0].table[d->rehashidx] = NULL;
        //到下一个rehashidx
        d->rehashidx++;
    }
    return 1;
}
/*
 *返回以毫秒为单位的时间戳
 *
 */
long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

/* Rehash for an amount of time between ms milliseconds and ms+1 milliseconds */
/*在给定时间内（ms），以100步为单位，对dict进行rehash
 *
 */
int dictRehashMilliseconds(dict *d, int ms) {
    long long start = timeInMilliseconds();
    int rehashes = 0;

    while(dictRehash(d,100)) {
        rehashes += 100;
        if (timeInMilliseconds()-start > ms) break;
    }
    return rehashes;
}

/* This function performs just a step of rehashing, and only if there are
 * no safe iterators bound to our hash table. When we have iterators in the
 * middle of a rehashing we can't mess with the two hash tables otherwise
 * some element can be missed or duplicated.
 *
 * 如果字典不存在安全迭代器的情况下，对字典进行单步rehash
 * 字典有安全迭代器的情况下不能进行rehash
 * 因为两种不同的迭代和修改可能会弄乱字典
 *
 * This function is called by common lookup or update operations in the
 * dictionary so that the hash table automatically migrates from H1 to H2
 * while it is actively used. 
 *
 * 这个函数被多个通用的查找、更新操作调用
 * 它可以在字典被调用的同时进行rehash
 *
 * */
//如果安全迭代器==0，则进行单步的rehash
static void _dictRehashStep(dict *d) {
    if (d->iterators == 0) dictRehash(d,1);
}


/* Add an element to the target hash table */
/*d：字典
 *key：键
 *val：值
 *返回成功或err
 *添加节点到字典
 *
 */
int dictAdd(dict *d, void *key, void *val)
{
    dictEntry *entry = dictAddRaw(d,key);

    if (!entry) return DICT_ERR;
    dictSetVal(d, entry, val);
    return DICT_OK;
}

/* Low level add. This function adds the entry but instead of setting
 * a value returns the dictEntry structure to the user, that will make
 * sure to fill the value field as he wishes.
 *
 * This function is also directly exposed to user API to be called
 * mainly in order to store non-pointers inside the hash value, example:
 *
 * entry = dictAddRaw(dict,mykey);
 * if (entry != NULL) dictSetSignedIntegerVal(entry,1000);
 *
 * Return values:
 *
 * If key already exists NULL is returned.
 * If key was added, the hash entry is returned to be manipulated by the caller.
 *
 * 尝试将键插入到字典中
 * 如果键已经存在字典中则返回NULL
 * 如果键不存在，那么程序将创建新的哈希节点
 * 将节点和键值关联，并插入到字典中，返回节点本身
 */
dictEntry *dictAddRaw(dict *d, void *key)
{
    int index;
    dictEntry *entry;
    dictht *ht;

    //如果正在进行rehash模式，进行一次单步rehash
    if (dictIsRehashing(d)) _dictRehashStep(d);

    /* Get the index of the new element, or -1 if
     * the element already exists. */
    if ((index = _dictKeyIndex(d, key)) == -1)
        return NULL;

    /* Allocate the memory and store the new entry */
    ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
    entry = zmalloc(sizeof(*entry));
    entry->next = ht->table[index];
    ht->table[index] = entry;
    ht->used++;

    /* Set the hash entry fields. */
    dictSetKey(d, entry, key);
    return entry;
}

/* Add an element, discarding the old if the key already exists.
 * Return 1 if the key was added from scratch, 0 if there was already an
 * element with such key and dictReplace() just performed a value update
 * operation.
 *
 * 如果键值对为全新添加，那么直接添加，返回1
 * 如果键值对是通过对原有的键值，则进行替换，并返回0
 *
 * */
int dictReplace(dict *d, void *key, void *val)
{
    dictEntry *entry, auxentry;

    /* Try to add the element. If the key
     * does not exists dictAdd will suceed. */
    //如果能够直接添加则直接加进去
    if (dictAdd(d, key, val) == DICT_OK)
        return 1;
    /* It already exists, get the entry */
    //运行到这，说明里面已经有相同的key了
    //查找键在字典的哪个节点
    entry = dictFind(d, key);
    /* Set the new value and free the old one. Note that it is important
     * to do that in this order, as the value may just be exactly the same
     * as the previous one. In this context, think to reference counting,
     * you want to increment (set), and then decrement (free), and not the
     * reverse. */
    //保存原有的节点值？
    auxentry = *entry;
    //重新设置val值
    dictSetVal(d, entry, val);
    //释放旧值？把val指针的地址释放掉？
    dictFreeVal(d, &auxentry);
    return 0;
}

/* dictReplaceRaw() is simply a version of dictAddRaw() that always
 * returns the hash entry of the specified key, even if the key already
 * exists and can't be added (in that case the entry of the already
 * existing key is returned.)
 *
 * See dictAddRaw() for more information.
 *
 * dictAddRaw()根据给定key释放存在，执行以下动作：
 *
 * 1）key已经存在，返回包含该key的字典节点
 * 2）key不存在，那么将key添加到字典
 *
 * 不论发生哪种情况：
 * dictAddRaw()总是返回包含给定key的字典节点
 *
 * */
dictEntry *dictReplaceRaw(dict *d, void *key) {
    dictEntry *entry = dictFind(d,key);

    return entry ? entry : dictAddRaw(d,key);
}

/* Search and remove an element */
/*
 * 查找并删除包含给定键的节点
 *
 * 参数nofree决定是否调用键和值的释放函数
 * 0 表示调用，1 表示不调用
 *
 * 找到并成功删除返回DICT_OK,没周到则返回DICT_ERR
 * 
 * T = O（1）
 */
static int dictGenericDelete(dict *d, const void *key, int nofree)
{
    unsigned int h, idx;
    dictEntry *he, *prevHe;
    int table;
    
    //哈希表为空则返回error(
    if (d->ht[0].size == 0) return DICT_ERR; /* d->ht[0].table is NULL */
    //rehash则进行单步rehash
    if (dictIsRehashing(d)) _dictRehashStep(d);
    //计算哈希值
    h = dictHashKey(d, key);
    // 遍历
    for (table = 0; table <= 1; table++) {
        //计算索引 值
        idx = h & d->ht[table].sizemask;
        //找到对应键的节点
        he = d->ht[table].table[idx];
        //
        prevHe = NULL;
        //遍历该hash链表
        while(he) {
            //key键相等的话
            if (dictCompareKeys(d, key, he->key)) {
                /* Unlink the element from the list */
                //如果He的前节点非空，则到指向he的下一个节点，
                //即跳过he
                if (prevHe)
                    prevHe->next = he->next;
                //如果He的前向节点为空，则直接把he的next作为头节点
                else
                    d->ht[table].table[idx] = he->next;
                //nofree为0则进行释放操作
                if (!nofree) {
                    dictFreeKey(d, he);
                    dictFreeVal(d, he);
                }
                //释放he节点
                zfree(he);
                //used--
                d->ht[table].used--;
                //找到即返回ok
                return DICT_OK;
            }
            //转移指针
            prevHe = he;
            he = he->next;
        }
        //如果不在rehash情况下，则不用检测ht1了
        if (!dictIsRehashing(d)) break;
    }
    //如果能运行到这，那么表明not found
    return DICT_ERR; /* not found */
}

//直接删除，释放键值内存
int dictDelete(dict *ht, const void *key) {
    //直接nofree=0
    return dictGenericDelete(ht,key,0);
}

//不释放键值的内存
int dictDeleteNoFree(dict *ht, const void *key) {
    //nofree=1
    return dictGenericDelete(ht,key,1);
}

/* Destroy an entire dictionary */
/*
 * 删除哈希表上的所有节点，并重置哈希表的各项属性
 *
 * T = O（N）
 *
 */
int _dictClear(dict *d, dictht *ht, void(callback)(void *)) {
    unsigned long i;

    /* Free all the elements */
    //遍历字典中的某个哈希表的所有节点，并删除
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        dictEntry *he, *nextHe;
        //如果有回调函数，则执行回调函数
        if (callback && (i & 65535) == 0) callback(d->privdata);
        //找到不为空的表
        if ((he = ht->table[i]) == NULL) continue;
        //对这个表里的哈希链表进行遍历，并删除释放
        while(he) {
            nextHe = he->next;
            dictFreeKey(d, he);
            dictFreeVal(d, he);
            zfree(he);
            ht->used--;
            he = nextHe;
        }
    }
    /* Free the table and the allocated cache structure */
    //释放这个哈希表结构
    zfree(ht->table);
    /* Re-initialize the table */
    //重置哈希表属性
    _dictReset(ht);
    return DICT_OK; /* never fails */
}

/* Clear & Release the hash table */
//删除并释放整个字典
void dictRelease(dict *d)
{
    _dictClear(d,&d->ht[0],NULL);
    _dictClear(d,&d->ht[1],NULL);
    zfree(d);
}

/*
 * *d:字典
 * *key:键值
 * 功能：返回所对应的键的节点
 *
 */
dictEntry *dictFind(dict *d, const void *key)
{
    dictEntry *he;
    unsigned int h, idx, table;

    //table为空则直接返回NULL
    if (d->ht[0].size == 0) return NULL; /* We don't have a table at all */
    //如果正在执行rehash，则执行rehash单步
    if (dictIsRehashing(d)) _dictRehashStep(d);
    //计算哈希值
    h = dictHashKey(d, key);
    //在两个表寻找
    for (table = 0; table <= 1; table++) {
        //计算索引值
        idx = h & d->ht[table].sizemask;
        //把所对应的节点赋值给he
        he = d->ht[table].table[idx];
        //遍历哈希链表，找出真正等于key的键
        while(he) {
            if (dictCompareKeys(d, key, he->key))
                return he;
            he = he->next;
        }
        //如果没有执行rehash的话，for就只用到ht[0]
        if (!dictIsRehashing(d)) return NULL;
    }
    return NULL;
}

/*
 *
 * *d:字典指针
 * *key：键指针
 * 返回：键对应的值
 * 功能：根据键返回对应的值，如果不存在，则返回NULL
 *
 */
void *dictFetchValue(dict *d, const void *key) {
    dictEntry *he;

    //找到对应key的节点，如果找不到则返回NULL给he
    he = dictFind(d,key);
    //判断he空不空，则返回对应的值或者NULL
    return he ? dictGetVal(he) : NULL;
}

/* A fingerprint is a 64 bit number that represents the state of the dictionary
 * at a given time, it's just a few dict properties xored together.
 * When an unsafe iterator is initialized, we get the dict fingerprint, and check
 * the fingerprint again when the iterator is released.
 * If the two fingerprints are different it means that the user of the iterator
 * performed forbidden operations against the dictionary while iterating. */

/*
 * 一个64位的指纹信息表示了字典的状态在某个时间内，它表示某个字典属性的异或值。
 * 当一个非安全的迭代器被初始化，我们获取字典的指纹信息，并且再一次检查指纹在迭代器
 * 被释放的时候。
 * 如果两次的指纹不同，它表示迭代器的使用者在迭代期间执行了违反字典操作的命令
 *
 */
long long dictFingerprint(dict *d) {
    long long integers[6], hash = 0;
    int j;

    //获取基础信息
    integers[0] = (long) d->ht[0].table;
    integers[1] = d->ht[0].size;
    integers[2] = d->ht[0].used;
    integers[3] = (long) d->ht[1].table;
    integers[4] = d->ht[1].size;
    integers[5] = d->ht[1].used;

    /* We hash N integers by summing every successive integer with the integer
     * hashing of the previous sum. Basically:
     *
     * Result = hash(hash(hash(int1)+int2)+int3) ...
     *
     * This way the same set of integers in a different order will (likely) hash
     * to a different number. */
    //计算hash值
    for (j = 0; j < 6; j++) {
        hash += integers[j];
        /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
        hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

/*
 * 创建并返回给定字典的不安全迭代器
 *
 * T = O（1）
 *
 */
dictIterator *dictGetIterator(dict *d)
{
    //定义并给内容迭代器
    dictIterator *iter = zmalloc(sizeof(*iter));

    //初始化
    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    return iter;
}

/*
 * 创建并返回给定节点的安全迭代器
 *
 * T = O(1)
 */
dictIterator *dictGetSafeIterator(dict *d) {
    //调用非安全的迭代器生成器
    dictIterator *i = dictGetIterator(d);
    //把其设置成1
    i->safe = 1;
    return i;
}


/*
 * 返回迭代器指向的当前节点
 *
 * 字典迭代完毕时，返回NULL
 *
 * T = O（1）
 */
dictEntry *dictNext(dictIterator *iter)
{
    while (1) {
        //进入这个循环有两种可能：
        //1）这是迭代器第一次运行
        //2）当前索引链表中的节点已经迭代完（NULL为链表的表尾）
        if (iter->entry == NULL) {

            // 指向被迭代的哈希表
            dictht *ht = &iter->d->ht[iter->table];

            //初次被迭代时执行
            if (iter->index == -1 && iter->table == 0) {
                //如果是安全迭代器，那么更新安全迭代器计算器
                if (iter->safe)
                    iter->d->iterators++;
                //如果是不安全迭代器，那么计算指纹
                else
                    iter->fingerprint = dictFingerprint(iter->d);
            }
            //更新索引
            iter->index++;

            //如果迭代器的当前索引大于当前被迭代的哈希表的大小
            //那么说明这个哈希表已经迭代完毕
            if (iter->index >= (long) ht->size) {
                //如果正在rehash的话，那么说明1号哈希表也正在使用中
                //那么继续对1号哈希表进行迭代
                if (dictIsRehashing(iter->d) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                    //如果没有rehash，那么迭代已经完成了
                } else {
                    break;
                }
            }

            //如果进行到这里，说明这个哈希表并未迭代完
            //更新几点指针，指向下个索引链表的表头节点
            iter->entry = ht->table[iter->index];
        } else {
            //执行到这里，说明程序正在迭代某个链表
            //将节点指针指向链表的下个节点
            iter->entry = iter->nextEntry;
        }

        //如果当前节点不为空，那么也记录下该节点的下个节点
        //因为安全迭代器有可能会将迭代器返回的当前节点删除
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }

    //迭代完毕
    return NULL;
}


/*
 *
 * 释放给定字典迭代器
 *
 * T = O(1)
 */
void dictReleaseIterator(dictIterator *iter)
{
    if (!(iter->index == -1 && iter->table == 0)) {
        // 释放安全迭代器时，安全迭代器计数器减一
        if (iter->safe)
            iter->d->iterators--;
        // 释放不安全迭代器需要验证指纹是否有变化
        else
            assert(iter->fingerprint == dictFingerprint(iter->d));
    }
    zfree(iter);
}

/* Return a random entry from the hash table. Useful to
 * implement randomized algorithms
 *
 * 随机返回字典中的任意一个节点。
 *
 * 可用于实现随机化算法
 *
 * 如果字典为空，返回NULL
 *
 * T = O（N）
 * */
dictEntry *dictGetRandomKey(dict *d)
{
    dictEntry *he, *orighe;
    unsigned int h;
    int listlen, listele;

    //如果字典存放的哈希表的大小为0
    //则返回NULL
    if (dictSize(d) == 0) return NULL;
    //如果正在rehash状态，则执行rehash单步
    if (dictIsRehashing(d)) _dictRehashStep(d);
    //如果进行rehash的话，则会有两个表
    if (dictIsRehashing(d)) {
        do {
            h = random() % (d->ht[0].size+d->ht[1].size);
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
                                      d->ht[0].table[h];
        } while(he == NULL);
    }
    //如果不进行rehash的话，则只有一个表
    else {
        do {
            //随机数用来计算哈希值
            h = random() & d->ht[0].sizemask;
            he = d->ht[0].table[h];
        } while(he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */

    //这里，已经随机获得一个he指向一个哈希链表了
    //程序将从这个链表中随机返回一个节点
    listlen = 0;
    orighe = he;
    //计算节点数量，T = O（1）
    while(he) {
        he = he->next;
        listlen++;
    }

    //随机计算出偏移量
    listele = random() % listlen;
    he = orighe;
    //按索引查找对应的节点
    while(listele--) he = he->next;
    return he;
}

/* Function to reverse bits. Algorithm from:
 * http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel */
static unsigned long rev(unsigned long v) {
    unsigned long s = 8 * sizeof(v); // bit size; must be power of 2
    unsigned long mask = ~0;
    while ((s >>= 1) > 0) {
        mask ^= (mask << s);
        v = ((v >> s) & mask) | ((v << s) & ~mask);
    }
    return v;
}

/* dictScan() is used to iterate over the elements of a dictionary.
 *
 * Iterating works the following way:
 *
 * 1) Initially you call the function using a cursor (v) value of 0.
 * 2) The function performs one step of the iteration, and returns the
 *    new cursor value you must use in the next call.
 * 3) When the returned cursor is 0, the iteration is complete.
 *
 * The function guarantees all elements present in the
 * dictionary get returned between the start and end of the iteration.
 * However it is possible some elements get returned multiple times.
 *
 * For every element returned, the callback argument 'fn' is
 * called with 'privdata' as first argument and the dictionary entry
 * 'de' as second argument.
 *
 * HOW IT WORKS.
 *
 * The iteration algorithm was designed by Pieter Noordhuis.
 * The main idea is to increment a cursor starting from the higher order
 * bits. That is, instead of incrementing the cursor normally, the bits
 * of the cursor are reversed, then the cursor is incremented, and finally
 * the bits are reversed again.
 *
 * This strategy is needed because the hash table may be resized between
 * iteration calls.
 *
 * dict.c hash tables are always power of two in size, and they
 * use chaining, so the position of an element in a given table is given
 * by computing the bitwise AND between Hash(key) and SIZE-1
 * (where SIZE-1 is always the mask that is equivalent to taking the rest
 *  of the division between the Hash of the key and SIZE).
 *
 * For example if the current hash table size is 16, the mask is
 * (in binary) 1111. The position of a key in the hash table will always be
 * the last four bits of the hash output, and so forth.
 *
 * WHAT HAPPENS IF THE TABLE CHANGES IN SIZE?
 *
 * If the hash table grows, elements can go anywhere in one multiple of
 * the old bucket: for example let's say we already iterated with
 * a 4 bit cursor 1100 (the mask is 1111 because hash table size = 16).
 *
 * If the hash table will be resized to 64 elements, then the new mask will
 * be 111111. The new buckets you obtain by substituting in ??1100
 * with either 0 or 1 can be targeted only by keys we already visited
 * when scanning the bucket 1100 in the smaller hash table.
 *
 * By iterating the higher bits first, because of the inverted counter, the
 * cursor does not need to restart if the table size gets bigger. It will
 * continue iterating using cursors without '1100' at the end, and also
 * without any other combination of the final 4 bits already explored.
 *
 * Similarly when the table size shrinks over time, for example going from
 * 16 to 8, if a combination of the lower three bits (the mask for size 8
 * is 111) were already completely explored, it would not be visited again
 * because we are sure we tried, for example, both 0111 and 1111 (all the
 * variations of the higher bit) so we don't need to test it again.
 *
 * WAIT... YOU HAVE *TWO* TABLES DURING REHASHING!
 *
 * Yes, this is true, but we always iterate the smaller table first, then
 * we test all the expansions of the current cursor into the larger
 * table. For example if the current cursor is 101 and we also have a
 * larger table of size 16, we also test (0)101 and (1)101 inside the larger
 * table. This reduces the problem back to having only one table, where
 * the larger one, if it exists, is just an expansion of the smaller one.
 *
 * LIMITATIONS
 *
 * This iterator is completely stateless, and this is a huge advantage,
 * including no additional memory used.
 *
 * The disadvantages resulting from this design are:
 *
 * 1) It is possible we return elements more than once. However this is usually
 *    easy to deal with in the application level.
 * 2) The iterator must return multiple elements per call, as it needs to always
 *    return all the keys chained in a given bucket, and all the expansions, so
 *    we are sure we don't miss keys moving during rehashing.
 * 3) The reverse cursor is somewhat hard to understand at first, but this
 *    comment is supposed to help.
 */
unsigned long dictScan(dict *d,
                       unsigned long v,
                       dictScanFunction *fn,
                       void *privdata)
{
    // 哈希表t0  t1
    dictht *t0, *t1;
    // 节点
    const dictEntry *de;
    unsigned long m0, m1;

    //字典的长度，
    //如果长度是0，则表示是没有哈希表的字典
    if (dictSize(d) == 0) return 0;

    //如果不处于rehash的状态
    if (!dictIsRehashing(d)) {
        //取ht0的地址给t0
        t0 = &(d->ht[0]);
        //取t0的掩码
        m0 = t0->sizemask;

        /* Emit entries at cursor */
        // v按位与 m0掩码读取哈希表中的节点
        de = t0->table[v & m0];
        //哈希链表
        while (de) {
            fn(privdata, de);
            de = de->next;
        }

        //迭代有两个哈希表的字典
    } else {

        // 指向两个哈希表
        t0 = &d->ht[0];
        t1 = &d->ht[1];

        /* Make sure t0 is the smaller and t1 is the bigger table */
        // 确保t0 比 t1 要小
        // 如果不满足则把他们交换
        if (t0->size > t1->size) {
            t0 = &d->ht[1];
            t1 = &d->ht[0];
        }

        // 记录掩码
        m0 = t0->sizemask;
        m1 = t1->sizemask;

        /* Emit entries at cursor */
        // 指向桶，并迭代桶中的所有节点
        de = t0->table[v & m0];
        while (de) {
            fn(privdata, de);
            de = de->next;
        }

        /* Iterate over indices in larger table that are the expansion
         * of the index pointed to by the cursor in the smaller table */
        // 迭代大表中的桶
        // 这些桶被索引的expansion所指向
        do {
            /* Emit entries at cursor */
            // 指向桶，并迭代桶中的所有节点
            de = t1->table[v & m1];
            while (de) {
                fn(privdata, de);
                de = de->next;
            }

            /* Increment bits not covered by the smaller mask */
            v = (((v | m0) + 1) & ~m0) | (v & m0);

            /* Continue while bits covered by mask difference is non-zero */
        } while (v & (m0 ^ m1));
    }

    /* Set unmasked bits so incrementing the reversed cursor
     * operates on the masked bits of the smaller table */
    v |= ~m0;

    /* Increment the reverse cursor */
    v = rev(v);
    v++;
    v = rev(v);

    return v;
}

/* ------------------------- private functions ------------------------------ */

/* Expand the hash table if needed
 *
 * 根据需要，初始化字典（的哈希表），或者对字典（的现有哈希表）进行扩展
 *
 * T = O（N）
 *
 *
 * */
static int _dictExpandIfNeeded(dict *d)
{
    /* Incremental rehashing already in progress. Return. */
    //如果已经处于rehash状态了，那就不用进行是否扩展的判断了
    if (dictIsRehashing(d)) return DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
    //如果这是一个空的字典，则扩展两个哈希表给它
    if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets.
     *
     * 以下两个条件之一为真，则对字典进行扩展：
     * 1）字典已使用节点数和字典大小之间的比率接近1:1
     *    并且dict_can_resize为真
     * 2）已使用节点数和字典大小之间的比率超过dict_force_resize_ratio
     * 
     * */
    if (d->ht[0].used >= d->ht[0].size &&
        (dict_can_resize ||
         d->ht[0].used/d->ht[0].size > dict_force_resize_ratio))
    {
        // 新哈希表的大小至少是目前已使用节点数的两倍
        return dictExpand(d, d->ht[0].used*2);
    }
    return DICT_OK;
}

/* Our hash table capability is a power of two */
//计算第一个大于size的（2的N次方的数)，用作哈希表的值
static unsigned long _dictNextPower(unsigned long size)
{
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) return LONG_MAX;
    while(1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Returns the index of a free slot that can be populated with
 * a hash entry for the given 'key'.
 * If the key already exists, -1 is returned.
 * 
 * 返回可以将key插入到哈希表的索引位置
 * 如果key已经存在于哈希表，那么返回-1
 *
 * Note that if we are in the process of rehashing the hash table, the
 * index is always returned in the context of the second (new) hash table.
 *
 * 注意，如果字典正在rehash，那么总是返回1号哈希表索引。
 * 因为在字典进行rehash时，新节点总是插入到1号哈希表
 *
 * T = O（N）
 *
 * */
static int _dictKeyIndex(dict *d, const void *key)
{
    unsigned int h, idx, table;
    dictEntry *he;

    /* Expand the hash table if needed */
    // 判断是否需要扩展，不需要的话则直接退出
    if (_dictExpandIfNeeded(d) == DICT_ERR)
        return -1;
    /* Compute the key hash value */
    // 计算哈希值
    h = dictHashKey(d, key);

    for (table = 0; table <= 1; table++) {
        //计算索引值
        idx = h & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
        //查找key是否存在
        he = d->ht[table].table[idx];
        while(he) {
            if (dictCompareKeys(d, key, he->key))
                return -1;
            he = he->next;
        }

        //运行到这说明已经结束了ht0的所有的节点的遍历，
        //并且key不在ht0里面
        //如果处于rehash状态，则进行下一轮遍历
        if (!dictIsRehashing(d)) break;
    }
    //返回可以加入哈希表的索引
    return idx;
}

/*
 *
 * 清空字典上的所有的哈希节点，并重置字典属性
 *
 * T = O（N）
 *
 */
void dictEmpty(dict *d, void(callback)(void*)) {
    // 删除两个哈希表上的所有节点
    // T = O（N）
    _dictClear(d,&d->ht[0],callback);
    _dictClear(d,&d->ht[1],callback);
    // 重置属性
    d->rehashidx = -1;
    d->iterators = 0;
}

/*
 * 开启自动rehash
 * 
 * T = O(N)
 */
void dictEnableResize(void) {
    dict_can_resize = 1;
}

/*
 * 关闭自动rehash
 *
 * T = O(N)
 */
void dictDisableResize(void) {
    dict_can_resize = 0;
}

#if 0

/* The following is code that we don't use for Redis currently, but that is part
of the library. */

/* ----------------------- Debugging ------------------------*/

#define DICT_STATS_VECTLEN 50
static void _dictPrintStatsHt(dictht *ht) {
    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN];

    if (ht->used == 0) {
        printf("No stats available for empty dictionaries\n");
        return;
    }

    for (i = 0; i < DICT_STATS_VECTLEN; i++) clvector[i] = 0;
    for (i = 0; i < ht->size; i++) {
        dictEntry *he;

        if (ht->table[i] == NULL) {
            clvector[0]++;
            continue;
        }
        slots++;
        /* For each hash entry on this slot... */
        chainlen = 0;
        he = ht->table[i];
        while(he) {
            chainlen++;
            he = he->next;
        }
        clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN-1)]++;
        if (chainlen > maxchainlen) maxchainlen = chainlen;
        totchainlen += chainlen;
    }
    printf("Hash table stats:\n");
    printf(" table size: %ld\n", ht->size);
    printf(" number of elements: %ld\n", ht->used);
    printf(" different slots: %ld\n", slots);
    printf(" max chain length: %ld\n", maxchainlen);
    printf(" avg chain length (counted): %.02f\n", (float)totchainlen/slots);
    printf(" avg chain length (computed): %.02f\n", (float)ht->used/slots);
    printf(" Chain length distribution:\n");
    for (i = 0; i < DICT_STATS_VECTLEN-1; i++) {
        if (clvector[i] == 0) continue;
        printf("   %s%ld: %ld (%.02f%%)\n",(i == DICT_STATS_VECTLEN-1)?">= ":"", i, clvector[i], ((float)clvector[i]/ht->size)*100);
    }
}

void dictPrintStats(dict *d) {
    _dictPrintStatsHt(&d->ht[0]);
    if (dictIsRehashing(d)) {
        printf("-- Rehashing into ht[1]:\n");
        _dictPrintStatsHt(&d->ht[1]);
    }
}

/* ----------------------- StringCopy Hash Table Type ------------------------*/

static unsigned int _dictStringCopyHTHashFunction(const void *key)
{
    return dictGenHashFunction(key, strlen(key));
}

static void *_dictStringDup(void *privdata, const void *key)
{
    int len = strlen(key);
    char *copy = zmalloc(len+1);
    DICT_NOTUSED(privdata);

    memcpy(copy, key, len);
    copy[len] = '\0';
    return copy;
}

static int _dictStringCopyHTKeyCompare(void *privdata, const void *key1,
        const void *key2)
{
    DICT_NOTUSED(privdata);

    return strcmp(key1, key2) == 0;
}

static void _dictStringDestructor(void *privdata, void *key)
{
    DICT_NOTUSED(privdata);

    zfree(key);
}

dictType dictTypeHeapStringCopyKey = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

/* This is like StringCopy but does not auto-duplicate the key.
 * It's used for intepreter's shared strings. */
dictType dictTypeHeapStrings = {
    _dictStringCopyHTHashFunction, /* hash function */
    NULL,                          /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

/* This is like StringCopy but also automatically handle dynamic
 * allocated C strings as values. */
dictType dictTypeHeapStringCopyKeyValue = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    _dictStringDup,                /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    _dictStringDestructor,         /* val destructor */
};
#endif

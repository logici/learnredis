# learnredis
learn for redis and add some note for redis

## version redis-2.8.17

## from https://mp.weixin.qq.com/s/GqSD_af_yUBfqprcZieWcA

### struct dict
dict字典总集    dictType字典类型，定义字典操作的一些方法
iterations当前迭代器，有安全迭代器和非安全迭代器
dictht[0] [1]哈希表0 旧表和 哈希表1 新表
dictEntry字典集合 存放真正的键值对，字典集合放在哈希表中。
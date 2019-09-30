# redis
1.test 测试
memtest.c 内存检测
redis_benchmark.c    用于redis性能测试的实现
redis_check_aof.c    用于更新日志检查的实现
redis_check_dump.c    用于本地数据库检查的实现
testhelp.c    一个c风格的小型测试框架

2 struct 结构体
adlist.c    用于对list的定义，它是个双向链表结构
dict.c    主要对于内存中的hash进行管理
sds.c    用于对字符串的定义
sparkine.c    一个拥有sample列表的序列
t_hash.c    hash在Server/Client中的应答操作。主要通过redisObject进行类型转换
t_list.c    list在Server/client中的应答操作。主要通过redisObject进行类型转换
t_set.c    set在Server/client中的应答操作。主要通过redisObject进行类型转换
t_string.c    string在Server/Client中的应答操作。主要通过redisObject进行类型转换
t_zset.c    zset在Server/Client中的应答操作。主要通过redisObject进行类型转换
ziplist.c    ziplist是一个类似于list的存储对象。它的原理类似于zipmap。
zipmap.c    zipmap是一个类似于hash的存储对象。

3.data 数据操作
aof.c    全称为append onlt file ，作用就是记录每次的写操作，在遇到断电等问题时
config.c    用于将配置文件redis.conf文件中的配置读取出来的属性通过程序到server
db.c    对于redis内存数据库的相关操作
multi.c    对于事务处理操作
rdb.c    对于redis本地数据库的相关操作，默认文件是dump.rdb(t通过配置文件获得)
replication.c    用于主从数据库的复制操作的实现

4 tool工具
bitops.c    位操作相关类
debug.c    用于测试时使用
endianconv.c    高低位转换，不同系统，高地位顺序不同
help.h    辅助于命令的提示信息
lzf_c.c    压缩算法系列
lzf_d.c    压缩算法系列
rand.c    用于产生随机数
release.c    用于产生随机数
sha1.c    sha加密算法的实现
util.c        通用工具方法
crc64.c    循环冗余校验

5 event事件
ae,c    用于redis的事件处理，包括句柄事件和超时事件。
ae_epoll.c    实现epoll系统调用的接口
ae_evport.c    实现evport系统调用的接口
ae_kqueue.c    实现kqueuex系统调用的接口
ae_select.c        实现select系统调用的接口

6 bashinfo    基本信息
asciilogo.c    redis的logo显示
version.h    定有redis的版本号

7 compatible    兼容
fmacros.h    兼容mac系统下的问题    
solarisfixes.h    兼容solary下的问题

8    main 主程序
redis.c    redis服务端程序
redis_cli.c    redis客户端程序

9    net 网路
anet.c    作为Server/Client通信的基础封装
networking.c    网络协议传输方法定义相关的都放在这个文件里

10    wrapper 封装类
bio.c    background I/O的意思，开启后台线程用
hyperloglog.c    一种日志类型
intset.c    整数范围内的使用set,并包含相关set操作
latency.c    延迟类
migrate.c    命令迁移类，包括命令的还算迁移
notify.c    通知类
object.c    用于创建和释放redisObject对象
pqsort.c    排序算法类
pubsub.c    用于订阅模式的实现，有点类似于client广播发送的方式
rio.c    定义的一个I/O类型
slowlog.c    一种日志类型的，与hyperloglog.c类型
sort.c    排序算法类，与pqsort.c使用的场景不同
syncio.c    用于同步Socket和文件I/O操作
zmalloc    关于redis的内存分配的封装实现

11 others
scripting.c
sentinel.c
setproctitle.c
valgrind.sh
redisassert.h

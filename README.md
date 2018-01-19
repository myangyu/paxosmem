介绍

    基于内存key-value的存储服务， 支持多节点备份容灾
    
特点

    强一致性
    高可用性
    高性能

使用
    
    1下载源码
    2编译
     gcc epollserver.c -o epoll ./dispatcher.c ./data/str.c -lcrypto -lssl -lm
     默认支持三节点备份 端口分别为 12000 12001 12002
     编译客户端
     gcc client.c -o client
    3分别启动服务
    ./epoll 12000
    ./epoll 12001
    ./epoll 12002
    4等待同步完成
    5客户端链接
     ./client
    6执行操作
     set 【key】 【value】
     
支持类型
    字符串
介绍

    基于内存key-value的存储服务， 支持多节点备份容灾
    
特点

    强一致性
    高可用性
    高性能

使用
    
    1 下载源码
    2 编译
     make install
    3 分别启动服务
     ./server conf/Config1.ini
     ./server conf/Config2.ini
     ./server conf/Config3.ini
    4等待同步完成
    5客户端链接
     ./client
    6执行操作
     set 【key】 【value】
     
支持类型
    字符串

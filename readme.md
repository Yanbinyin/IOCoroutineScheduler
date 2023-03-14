# IO Coroutine Scheduler

## 1. Introduction

本仓库主体是一个高性能的IO协程库，做的过程中写了很多注释，push到GitHub方便自己后面查阅。

## 2. 主要由以下部分组成：

### 信号量、锁模块
### 日志系统
### 配置系统
### 线程模块
### 协程模块
### IO协程调度模块
### hook模块
### 定时器模块
### 数据压缩、序列化与反序列化模块

本项目不含复杂依赖，无论在哪个文件夹，安装gcc、cmake、yaml、openssl后可以直接使用下面命令编译使用。

```
mkdir build && cd build
cmake ..
make
```

## 3. Folder structure
+ bin：二进制文件
+ build：中间文件
+ cmake：cmake函数文件夹
+ CMakeLists.txt：cmake的定义文件
+ lib：库的输出路径
+ server-bin：源代码路径
+ tests：测试文件夹

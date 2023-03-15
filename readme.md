# IO Coroutine Scheduler

**First of all. thanks for your reading or star. If you have any quesetions, your issue or inqury are welcome.**

首先。谢谢你的阅读或star。有任何问题，欢迎issue或提问。

## 1. Introduction

The main body of the repo is a high performance I/O coroutine library, to achieve high concurrency, high efficiency coroutine scheduling.

- Design thread pool and coroutine queue, realize asymmetric coroutine and coroutine switch based on ucontext_t, and realize coroutine scheduler.
- Implements the timing function based on epoll timeout, encapsulates all file descriptors of the scheduler, and solves the problem of high CPU usage caused by busy waiting in idle state.
- hook System call interface, enabling apis that do not have asynchronous functions (such as API at the bottom of the system, Socket, IO, and sleep-related apis) to display asynchronous functions.
- Encapsulates epoll and designs I/O coroutine scheduler to achieve high concurrency and high efficiency coroutine scheduling.

本仓库主体是一个高性能的IO协程库，实现高并发、高效率的协程调度。

- 设计线程池和协程队列，基于ucontext_t实现非对称协程和协程切换，实现协程调度器。
- 基于epoll 超时实现定时功能，封装调度器所有的文件描述符，解决在idle状态下忙等待导致CPU占用率高的问题。
- hook系统调用接口，使不具备异步功能的API（如系统底层API、Socket、IO、sleep相关的API等）展现出异步的功能。
- 封装了epoll、设计I/O协程调度器，实现高并发、高效率的协程调度。

## 2. Frame Struct

- ### Semaphore & Lock(信号量、锁模块)
- ### Log(日志系统)
- ### Config(配置系统)
- ### Thread(线程模块)
- ### Corountine(协程模块)
- ### IO Corountine Scheduling(IO协程调度模块)
- ### Hook(hook模块)
- ### Timer(定时器模块)
- ### Data Processing(数据压缩、序列化与反序列化模块)
- ### Test(测试模块)

## 3. Complie

This project does not contain any anycomplex dependencies. After installing gcc, cmake, yaml, and openssl, you can clone and directly use the following commands to compile in any folder.

本项目不含复杂依赖，安装gcc、cmake、yaml、openssl后,可以在任意文件夹下clone并使用下面命令编译。

```
mkdir build && cd build
cmake ..
make
```

## 4. Folder structure

+ bin：Binary files
+ build：Build filess
+ cmake：Cmake include files
+ CMakeLists.txt：cmake
+ lib：The output path of the library
+ server-bin：Source code
+ tests：Testing code

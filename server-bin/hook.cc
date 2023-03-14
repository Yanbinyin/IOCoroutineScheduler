#include "hook.h"
#include <dlfcn.h>

#include "config.h"
#include "log.h"
#include "coroutine.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "macro.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

//block1 准备：完成HOOK的初始化
//HOOK工作需要在程序真正开始执行前完成，即：在进入main()之前执行完毕
namespace sylar{

    //正常的connect函数是没有超时的，但是有必要设置一下
    static sylar::ConfigVar<int>::ptr g_tcp_connect_timeout = sylar::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

    static thread_local bool t_hook_enable = false;

    #define HOOK_FUN(XX) \
        XX(sleep) \
        XX(usleep) \
        XX(nanosleep) \
        XX(socket) \
        XX(connect) \
        XX(accept) \
        XX(read) \
        XX(readv) \
        XX(recv) \
        XX(recvfrom) \
        XX(recvmsg) \
        XX(write) \
        XX(writev) \
        XX(send) \
        XX(sendto) \
        XX(sendmsg) \
        XX(close) \
        XX(fcntl) \
        XX(ioctl) \
        XX(getsockopt) \
        XX(setsockopt)
    
    /*//power:方法二：利用C特性，使用__attribute__((constructor))声明，将一个函数置在main()之前执行
    void hook_init() __attribute__((constructor));

    void hook_init(){
        static bool is_inited = false;
        if(is_inited)
            return;
    #define XX(name)    name ## _f = (name ## _func)dlsym(RTLD_NEXT, #name);
        HOOK_FUNC(XX)
    #undef XX
    }*/
    void hook_init(){
        static bool is_inited = false;
        if(is_inited)
            return;
    //power: hook.h里面extern声明的变量，在这里通过宏定义的方式进行定义
    #define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    //#define XX(sleep) sleep_f = (sleep_fun)dlsym(RTLD_NEXT, sleep);
        HOOK_FUN(XX);
    #undef XX
    }


    //power:方法一：利用C++特性，构造结构体变量，声明为static，利用其结构体的构造函数执行HOOK初始化
    static uint64_t s_connect_timeout = -1;
    
    struct _HookIniter{
        _HookIniter(){
            hook_init(); //初始化hook
            s_connect_timeout = g_tcp_connect_timeout->getValue();
            g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value){
                    SYLAR_LOG_INFO(g_logger) << "tcp connect timeout changed from "
                                             << old_value << " to " << new_value;
                    s_connect_timeout = new_value;
            });
        }
    };

    static _HookIniter s_hook_initer;


    //辅助函数
    bool is_hook_enable(){
        return t_hook_enable;
    }

    void set_hook_enable(bool flag){
        t_hook_enable = flag;
    }

}





//block2: hook系统函数，使用我们自定义的函数，替换系统调用
struct timer_info{
    int cancelled = 0;
};


//P45结束定义，P46开始讲解
/*SO_RCVTIMEO和SO_SNDTIMEO，分别用来设置socket接收数据超时时间和发送数据超时时间
这两个选项仅对与数据收发相关的系统调用有效，这些系统调用包括：send, sendmsg, recv, recvmsg, accept, connect 。
这两个选项设置后，若超时， 返回-1，并设置errno为EAGAIN或EWOULDBLOCK
其中connect超时的话，也是返回-1, 但errno设置为EINPROGRESS
*/
template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name, uint32_t event, int timeout_so, Args&&... args){
    if(!sylar::t_hook_enable)
        return fun(fd, std::forward<Args>(args)...);

    SYLAR_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << ">";

    //1. 从FdManager中通过get()获取当前文件描述符fd的对象FdCtx
    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    
    //a. FdManger不存在当前的文件描述符fd，我们认为它不是一个socket，执行原来的系统调用
    if(!ctx)
        return fun(fd, std::forward<Args>(args)...);

    //b. 该描述符是socket但是已经被关闭，就返回错误
    if(ctx->isClose()){
        errno = EBADF;
        return -1;
    }

    //c. 该描述符明确不是socket 或者 已经被设置为NonBlock非阻塞状态，执行原来的系统调用
    if(!ctx->isSocket() || ctx->getUserNonblock())
        return fun(fd, std::forward<Args>(args)...);
    
    //后面hook住了
    
    //2. 获取当前套接字上的读/写超时时间getTimeout()，并且通过shared_ptr<>设置一个定时器的条件（结构体struct timer_info）用于判定定时器是否是超时被执行
    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    //3. 对读写IO执行一次IO操作，返回值有效直接到函数末尾的return
    ssize_t n = fun(fd, std::forward<Args>(args)...);

    //a. 判断返回值是否出错，是否属于被中断errno = EINTR。如果是，需要对重试IO操作
    while(n == -1 && errno == EINTR) //EINTR: 系统中断
        n = fun(fd, std::forward<Args>(args)...);

    // b. 判断返回值是否出错，是否属于阻塞状态errno = EAGAIN。如果是：
    if(n == -1 && errno == EAGAIN){ //again: 异步操作 
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo); //条件变量
        
        //ⅰ. 添加一个条件定时器ConditionTimer，将 2.中的设置的条件赋值给弱指针weak_ptr<>管理。定时器到时后，取消对应fd上的事件并且强制触发回调函数
        if(to != (uint64_t)-1){ //超时时间为-1说明设置了超时，这么长时间没触发，就放进定时器中主动触发
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event](){
                auto t = winfo.lock();
                if(!t || t->cancelled) //空说明执行了
                    return;
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, (sylar::IOManager::Event)(event));
            }, winfo);
        }
        
        //ⅱ. IOManagerIO调度器为fd添加所需的读/写事件，设置异步回调
        int rt = iom->addEvent(fd, (sylar::IOManager::Event)(event));
        if(SYLAR_UNLIKELY(rt)){ //添加失败
            SYLAR_LOG_ERROR(g_logger) << hook_fun_name << " addEvent(" << fd << ", " << event << ")";
            if(timer)
                timer->cancel();
            return -1;
        }
        else{
            //ⅲ. 将当前协程挂起让出执行权Coroutine::YieldToHold()
            sylar::Fiber::YieldToHold();

            /*//4. 等待协程切回（从Coroutine::YieldToHold()返回），读事件READ返回继续执行read；写事件WRITE返回继续执行write
            ● 切回的条件：
            a. IO没有数据到达，条件定时器超时强制唤醒：
                取消全部套接字全部事件cancelEvent()，没有指定回调函数function< > cb，主动触发事件triggerEvent()默认将当前的协程作为任务加入队列，执行上一次没执行完的协程内容
            b. 定时器还没有超时，IO活跃有数据到达触发回调
                根据epoll_wait()带回的活跃IO信息epoll_event拿到对应的套接字对象FdCtx，主动触发事件triggerEvent()默认将当前的协程作为任务加入队列，执行上一次没执行完的协程内容
            */

            //5. 检查之前的条件定时器是否还存在，存在就要将其取消，否则还会超时触发
            if(timer)
                timer->cancel(); 

            //6. 检查超时条件是否有值，有值说明还没释放，返回错误
            if(tinfo->cancelled){
                errno = tinfo->cancelled;
                return -1;
            }

            //7. goto RETRY继续IO操作，读写数据
            goto retry;
        }
    }

    return n;
}


extern "C"{
    //block: 2.1 HOOKsleep、usleep、nanosleep系统函数
    #define XX(name) name ## _fun name ## _f = nullptr;
    //#define XX(sleep) sleep_fun sleep_f = nullptr;
        HOOK_FUN(XX);
    #undef XX

    unsigned int sleep(unsigned int seconds){
        //如果当前线程没有被hook就返回旧的系统调用执行
        if(!sylar::t_hook_enable)
            return sleep_f(seconds);

        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        //power: 指针//inline void sylar::Scheduler::schedule<sylar::Fiber::ptr>(sylar::Fiber::ptr fc, int thread)
        //iom->addTimer(usec/1000, iom->schedule(fiber, -1));
        iom->addTimer(seconds * 1000, std::bind((void(sylar::Scheduler::*)
                    (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
                    ,iom, fiber, -1)); 
        sylar::Fiber::YieldToHold();
        return 0;
    }

    int usleep(useconds_t usec){
        if(!sylar::t_hook_enable)
            return usleep_f(usec);
        
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        iom->addTimer(usec / 1000, std::bind((void(sylar::Scheduler::*)
                (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
                ,iom, fiber, -1));
        sylar::Fiber::YieldToHold();
        return 0;
    }

    int nanosleep(const struct timespec *req, struct timespec *rem){
        if(!sylar::t_hook_enable)
            return nanosleep_f(req, rem);

        int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 /1000;
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        iom->addTimer(timeout_ms, std::bind((void(sylar::Scheduler::*)
                (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
                ,iom, fiber, -1));
        sylar::Fiber::YieldToHold();
        return 0;
    }




    //block: 2.2 HOOK socket  API
    /*目的：通过截取系统事件对应的系统调用函数，在其函数中重新自定义我们所需要的一些操作。
    结合本使用案例，就是需要完成socket原有API的功能基础上，配合class:FdCtx套接字状态类以及class:FdManager套接字管理类，能够在用户态层面管理套接字的相关属性、信息

    2.1 读/写相关API
        统一使用上面的函数模板do_io()处理，因为这些API几乎都是容易阻塞的，而阻塞就需要HOOK来和协程结合，达到同步编程异步效率的效果。
            ● accept
            ● read/write
            ● readv/writev
            ● recv/send
            ● recvfrom/sendto
            ● recvmsg/sendmsg

    2.2 部分socket相关API
        比较特殊，需要单独处理，不能使用do_io()来概括
    */

    //功能：创建套接字的同时，需要把系统返回的fd通过FdManager在用户态维护在FdCtx中
    int socket(int domain, int type, int protocol){
        if(!sylar::t_hook_enable)
            return socket_f(domain, type, protocol);
        
        int fd = socket_f(domain, type, protocol);
        if(fd == -1)
            return fd;

        //由FdManager创建一个fd
        sylar::FdMgr::GetInstance()->get(fd, true);
        return fd;
    }

    /*2.2.2.4 connect()/connect_with_timeout()（重点）
    功能：连接服务器，原本的系统调用不具备手动设置超时的功能，导致默认的超时设置时间太长，
        不同环境下时长不同，在75s到几分钟之间；而且远端服务器不存在或者未开启将一直处于阻塞（套接字为阻塞状态时）

    冷门易错点①：通过man查看Linux手册，connect返回的错误码中有一个是EINPROGRESS，对应的描述大致是：在非阻塞模式下，如果连接不能马上建立成功就会返回该错误码。
        返回该错误码，可以通过使用select或者poll来查看套接字是否可写，如果可写，再调用getsockopt来获取套接字层的错误码errno来确定连接是否真的建立成功，
        如果getsockopt返回的错误码是0则表示连接真的建立成功，否则返回对应失败的错误码。

    冷门易错点②：针对select/poll/epoll监测管理的IO上的可读/可写事件，有两条规则：
        1. 当连接建立成功时，套接口描述符变成可写（连接建立时，写缓冲区空闲，所以可写）
        2. 当连接建立出错时，套接口描述符变成既可读又可写（由于有未决的错误，从而可读又可写）

    实现connect()的超时设置有如下思路：

    1. 利用select()/poll()检测socket。
        a. 创建一个socket
        b. 将套接字设置为NonBlock非阻塞状态
        c. 执行connect()之后，利用select检测该套接字上的是否可写，来判断connect()的结果
        d. select()返回后还要进一步判断，getsockopt()检测连接是否有问题。有问题返回错误
        e. 没有问题返回0，将socket重新设置为非阻塞的
    2. 本例利用条件定时器ConditionTimer，配合配置系统设定一个connect_with_timeout的超时时间
        a. 当系统调用connect()没马上建立连接时：返回的是EINPROGRESS，利用ConditionTimer条件定时器，超时（默认5000ms）触发强行唤醒协程执行
        b. connect()成功时，epoll管理的IO会触发WRITE写事件；connect仍然存在异常时，epoll管理的IO会同时触发READ/WRITE可读可写事件，
            触发异步回调唤醒协程执行。协程切回后使用getsockopt()进一步判断连接是否真的成功建立
    3. connect()返回错误码中还有一个是EISCONN，表示连接已经建立。
        我们可以再一次调用connect()函数，如果返回的错误码是EISCONN，则表示连接建立成功，否则认为连接建立失败
    */
    /*
    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
        The connect() system call connects the socket referred to by the file descriptor sockfd to the address specified by addr. 
        The addrlen argument specifies the size of addr.
    */
    int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms){
        if(!sylar::t_hook_enable)
            return connect_f(fd, addr, addrlen);
        
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
        if(!ctx || ctx->isClose()){
            errno = EBADF;
            return -1;
        }

        //非socket
        if(!ctx->isSocket())
            return connect_f(fd, addr, addrlen);

        //用户主动设置非阻塞
        if(ctx->getUserNonblock())
            return connect_f(fd, addr, addrlen);
        
        //创建socket时候已经设置为 非阻塞的 因此这里不会阻塞
        int n = connect_f(fd, addr, addrlen);
        //创建成功返回0
        if(n == 0)
            return 0;
        //不成功返回非0
        else if(n != -1 || errno != EINPROGRESS)
            return n;

        /*EINPROGRESS：
            在非阻塞模式下，如果连接不能马上建立成功就会返回该错误码。返回该错误码，可以通过使用select或者poll来查看套接字是否可写，
            如果可写，再调用getsockopt来获取套接字层的错误码errno来确定连接是否真的建立成功，
            如果getsockopt返回的错误码是0则表示连接真的建立成功，否则返回对应失败的错误码。
        */
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::shared_ptr<timer_info> tinfo(new timer_info);
        std::weak_ptr<timer_info> winfo(tinfo);

        if(timeout_ms != (uint64_t)-1){
            timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom](){
                    auto t = winfo.lock();
                    if(!t || t->cancelled){
                        return;
                    }
                    t->cancelled = ETIMEDOUT;
                    iom->cancelEvent(fd, sylar::IOManager::WRITE);
                }, winfo);
        }

        //添加写事件是因为 connect成功后马上可写
        int rt = iom->addEvent(fd, sylar::IOManager::WRITE);
        if(rt == 0){
            sylar::Fiber::YieldToHold(); //切出
            if(timer){ //切回
                timer->cancel();
            }
            if(tinfo->cancelled){
                errno = tinfo->cancelled;
                return -1;
            }
        } else{
            if(timer){
                timer->cancel();
            }
            SYLAR_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
        }

        //协程切回后 检查一下socket上是否有错误  才能最终判断连接是否建立
        int error = 0;
        socklen_t len = sizeof(int);
        if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)){
            return -1;
        }

        //检测sockfd 上是否有错误发生 有就通过int error变量带回
        //没有错误才是真的建立连接成功
        if(!error){
            return 0;
        }else{
            errno = error;
            return -1;
        }
    }
    //connect  较为复杂
    //connect与其他IO不同的在于它一旦connect完，当它写的时候他已经连成功了，不需要再做其他的IO操作
    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
        return connect_with_timeout(sockfd, addr, addrlen, sylar::s_connect_timeout);
    }

    //功能：接受客户端连接返回通信套接字，要把系统返回的fd通过FdManager在用户态维护在FdCtx中
    //由于accept本身是必然的会阻塞的，因此可以使用do_io()来HOOK它，和socket()类似
    int accept(int s, struct sockaddr *addr, socklen_t *addrlen){
        int fd = do_io(s, accept_f, "accept", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
        if(fd >= 0)
            sylar::FdMgr::GetInstance()->get(fd, true); //把新建立的通信套接字加入到FdManager中去管理
        return fd;
    }

    ssize_t read(int fd, void *buf, size_t count){
        //do_io(int fd, OriginFun fun, const char* hook_fun_name, uint32_t event, int timeout_so, Args&&... args){
        return do_io(fd, read_f, "read", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);
    }

    ssize_t readv(int fd, const struct iovec *iov, int iovcnt){
        return do_io(fd, readv_f, "readv", sylar::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
    }

    ssize_t recv(int sockfd, void *buf, size_t len, int flags){
        return do_io(sockfd, recv_f, "recv", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
    }

    ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
        return do_io(sockfd, recvfrom_f, "recvfrom", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
    }

    ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags){
        return do_io(sockfd, recvmsg_f, "recvmsg", sylar::IOManager::READ, SO_RCVTIMEO, msg, flags);
    }

    ssize_t write(int fd, const void *buf, size_t count){
        return do_io(fd, write_f, "write", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);
    }

    ssize_t writev(int fd, const struct iovec *iov, int iovcnt){
        return do_io(fd, writev_f, "writev", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
    }

    ssize_t send(int s, const void *msg, size_t len, int flags){
        return do_io(s, send_f, "send", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
    }

    ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen){
        return do_io(s, sendto_f, "sendto", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
    }

    ssize_t sendmsg(int s, const struct msghdr *msg, int flags){
        return do_io(s, sendmsg_f, "sendmsg", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
    }

    //功能：关闭文件描述符，要使用系统调用的之前，需要检查对应的fd是否存在于FdManager中，存在需要先将其删除，在关闭
    int close(int fd){
        if(!sylar::t_hook_enable)
            return close_f(fd);        

        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
        
        //如果是socket //hack: 为什么不空就是socket
        if(ctx){
            auto iom = sylar::IOManager::GetThis();
            if(iom)
                iom->cancelAll(fd);
            sylar::FdMgr::GetInstance()->del(fd);
        }
        return close_f(fd);
    }


    //功能：设置/获取系统fd上的相关状态。同时还要将状态同步到用户态的FdCtx上
    /*小技巧：
        HOOK fcntl()需要把它内部所有标志位都罗列重写，否则导致部分功能不可用。
        通过man查看Linux手册，按照传参类型进行一个分类，重点实现与我们代码模块相关的标志位，忽略其他无关的标志位，复用减少代码量，
        按int、void、锁相关struct flock*、进程组相关struct f_owner_ex *进行分类

    ● 重点实现：
    1. F_SETFL设置文件状态
        该标志位为int，通过可变参数传递，设置当前用户态FdCtx的信息，并设置系统中fd的状态是否是非阻塞的O_NONBLOCK
    2. F_GETFL获取文件状态
        该标志位为void，直接调用系统调用，根据传回的结果，又将用户态FdCtx对象的信息更新即可
    */
    int fcntl(int fd, int cmd, ... /* arg */ ){
        va_list va;
        va_start(va, cmd);
        switch(cmd){
            //int
            case F_SETFL: //设置文件状态
               {
                    int arg = va_arg(va, int);
                    va_end(va);
                    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
                    if(!ctx || ctx->isClose() || !ctx->isSocket())
                        return fcntl_f(fd, cmd, arg);
                    
                    ctx->setUserNonblock(arg & O_NONBLOCK);
                    if(ctx->getSysNonblock())
                        arg |= O_NONBLOCK;
                    else
                        arg &= ~O_NONBLOCK;
                    
                    return fcntl_f(fd, cmd, arg);
                }
                break;
            case F_GETFL: //获取文件状态
               {
                    va_end(va);
                    int arg = fcntl_f(fd, cmd);
                    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
                    if(!ctx || ctx->isClose() || !ctx->isSocket())
                        return arg;
                    if(ctx->getUserNonblock())
                        return arg | O_NONBLOCK;
                    else
                        return arg & ~O_NONBLOCK;
                    
                }
                break;

            case F_DUPFD:
            case F_DUPFD_CLOEXEC:
            case F_SETFD:
            case F_SETOWN:
            case F_SETSIG:
            case F_SETLEASE:
            case F_NOTIFY:
    #ifdef F_SETPIPE_SZ
            case F_SETPIPE_SZ:
    #endif
               {
                    int arg = va_arg(va, int);
                    va_end(va);
                    return fcntl_f(fd, cmd, arg); 
                }
                break;
            
            //void
            case F_GETFD:
            case F_GETOWN:
            case F_GETSIG:
            case F_GETLEASE:
    #ifdef F_GETPIPE_SZ
            case F_GETPIPE_SZ:
    #endif
               {
                    va_end(va);
                    return fcntl_f(fd, cmd);
                }
                break;
            
            //flock
            case F_SETLK:
            case F_SETLKW:
            case F_GETLK:
               {
                    struct flock* arg = va_arg(va, struct flock*);
                    va_end(va);
                    return fcntl_f(fd, cmd, arg);
                }
                break;
            /*进程组*/
            case F_GETOWN_EX:
            case F_SETOWN_EX:
               {
                    struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                    va_end(va);
                    return fcntl_f(fd, cmd, arg);
                }
                break;
            default:
                va_end(va);
                return fcntl_f(fd, cmd);
        }
    }

    //功能：作为IO操作的杂项补充。主要关注套接字的非阻塞状态，设置该状态时候用户态FdCtx的信息也改
    int ioctl(int d, unsigned long int request, ...){
        va_list va;
        va_start(va, request);
        void* arg = va_arg(va, void*);
        va_end(va);

        if(FIONBIO == request){
            bool user_nonblock = !!*(int*)arg;
            sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(d);
            if(!ctx || ctx->isClose() || !ctx->isSocket()){
                return ioctl_f(d, request, arg);
            }
            ctx->setUserNonblock(user_nonblock);
        }
        return ioctl_f(d, request, arg);
    }


    //功能：设置/获取套接字的设置与属性。getsockopt()不需要进行HOOK操作;
    //    setsockopt()主要关注套接字收数据超时SO_RCVTIMEO/发数据超时SO_SNDTIMEO的设置，需要将信息也同步至用户态FdCtx
    int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen){
        return getsockopt_f(sockfd, level, optname, optval, optlen);
    }

    int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen){
        if(!sylar::t_hook_enable){
            return setsockopt_f(sockfd, level, optname, optval, optlen);
        }
        if(level == SOL_SOCKET){
            if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO){
                sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(sockfd);
                if(ctx){
                    const timeval* v = (const timeval*)optval;
                    ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
                }
            }
        }
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }

}

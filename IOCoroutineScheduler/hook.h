//函数封装
/*
实现C、C++混合编程该怎么办呢？让编译后的函数符号一致： extern "C";
extern “C”的作用就是告诉C++编译器，这是一个用C写成的库文件，请用C的方式来链接它们，函数编译产生的函数名不带签名信息;
将指定的函数用C规则编译（注意，除了函数重载外，extern “C”不影响C++的其他特性）。
*/

#ifndef __SYLAR_HOOK_H__
#define __SYLAR_HOOK_H__

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>



//准备：提供两个接口控制当前线程的HOOK状态
namespace bin {    
    bool is_hook_enable();              //当前线程是否hook
    void set_hook_enable(bool flag);    //设置当前线程的hook状态
}



extern "C" {
    //sleep
    typedef unsigned int (*sleep_fun)(unsigned int seconds);
    extern sleep_fun sleep_f;
    //add: sleep_f 在这里只是声明了类型，将在hook.cc中的宏定义HOOK_FUN(XX)中直接定义（定义的时候直接使用了变量名），没有规定类型，这里已经规定了

    typedef int (*usleep_fun)(useconds_t usec);
    extern usleep_fun usleep_f;

    typedef int (*nanosleep_fun)(const struct timespec *req, struct timespec *rem);
    extern nanosleep_fun nanosleep_f;

    //socket
    typedef int (*socket_fun)(int domain, int type, int protocol);
    extern socket_fun socket_f;

    typedef int (*connect_fun)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    extern connect_fun connect_f;

    typedef int (*accept_fun)(int s, struct sockaddr *addr, socklen_t *addrlen);
    extern accept_fun accept_f;

    //read
    typedef ssize_t (*read_fun)(int fd, void *buf, size_t count);
    extern read_fun read_f;

    typedef ssize_t (*readv_fun)(int fd, const struct iovec *iov, int iovcnt);
    extern readv_fun readv_f;

    typedef ssize_t (*recv_fun)(int sockfd, void *buf, size_t len, int flags);
    extern recv_fun recv_f;

    typedef ssize_t (*recvfrom_fun)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
    extern recvfrom_fun recvfrom_f;

    typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr *msg, int flags);
    extern recvmsg_fun recvmsg_f;

    //write
    typedef ssize_t (*write_fun)(int fd, const void *buf, size_t count);
    extern write_fun write_f;

    typedef ssize_t (*writev_fun)(int fd, const struct iovec *iov, int iovcnt);
    extern writev_fun writev_f;

    typedef ssize_t (*send_fun)(int s, const void *msg, size_t len, int flags);
    extern send_fun send_f;

    typedef ssize_t (*sendto_fun)(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
    extern sendto_fun sendto_f;

    typedef ssize_t (*sendmsg_fun)(int s, const struct msghdr *msg, int flags);
    extern sendmsg_fun sendmsg_f;

    typedef int (*close_fun)(int fd);
    extern close_fun close_f;

    //
    typedef int (*fcntl_fun)(int fd, int cmd, ... /* arg */ );
    extern fcntl_fun fcntl_f;

    typedef int (*ioctl_fun)(int d, unsigned long int request, ...);
    extern ioctl_fun ioctl_f;

    typedef int (*getsockopt_fun)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
    extern getsockopt_fun getsockopt_f;

    typedef int (*setsockopt_fun)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
    extern setsockopt_fun setsockopt_f;

    extern int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms);

}

#endif

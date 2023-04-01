#include "IOCoroutineScheduler/bin.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>

bin::Logger::ptr g_logger = BIN_LOG_NAME("root");


//block1: 基础测试
#if 0
void func1(){
    BIN_LOG_INFO(g_logger) << "func1 work!!!!!!!!!!";
}

void func2(){
    BIN_LOG_INFO(g_logger) << "func2 work!!!!!!!!!!";
}

int main()
{
    bin::Thread::SetName("test");
    //创建IO调度器
    BIN_LOG_INFO(g_logger) << "test begin";
    bin::IOManager iom;
	
    //添加函数任务
    BIN_LOG_INFO(g_logger) << "add func";
    iom.schedule(&func1);
	
    //添加协程任务
    BIN_LOG_INFO(g_logger) << "add Fiber";
    bin::Fiber::ptr cor(new bin::Fiber(&func2));
    iom.schedule(cor);

    BIN_LOG_INFO(g_logger) << "test end";


    return 0;
}
#endif


/*int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

DESCRIPTION
    The connect() system call connects the socket referred to by the file descriptor sockfd to the address specified by addr.

RETURN VALUE
    If the connection or binding succeeds, zero is returned.  On error, -1 is returned, and errno is set appropriately.
*/

//block2: 进阶测试
#if 1
int sock = 0;

void test_fiber(){
    BIN_LOG_INFO(g_logger) << "test_fiber sock=" << sock;

    //sleep(5);

    //close(sock);
    //bin::IOManager::GetThis()->cancelAll(sock);

    //IPv4  TCP  无通信协议
    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK); //设置成异步的

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;  //IPv4
    addr.sin_port = htons(80);  //端口
    //convert IPv4 and IPv6 addresses from text to binary form  then copies the network address structure to addr.sin...
    inet_pton(AF_INET, "39.156.66.10", &addr.sin_addr.s_addr);
    
    if(!connect(sock, (const sockaddr*)&addr, sizeof(addr))){
    }else if(errno == EINPROGRESS){
        BIN_LOG_INFO(g_logger) << "add event errno=" << errno << " " << strerror(errno);
        bin::IOManager::GetThis()->addEvent(sock, bin::IOManager::READ
                    ,[](){ BIN_LOG_INFO(g_logger) << "read callback"; });
        bin::IOManager::GetThis()->addEvent(sock, bin::IOManager::WRITE, [](){
            BIN_LOG_INFO(g_logger) << "write callback";
            //close(sock);
            bin::IOManager::GetThis()->cancelEvent(sock, bin::IOManager::READ);
            close(sock);
        });
    }else{
        BIN_LOG_INFO(g_logger) << "else " << errno << " " << strerror(errno);
    }
}

void test1(){
    std::cout << "EPOLLIN=" << EPOLLIN << " EPOLLOUT=" << EPOLLOUT << std::endl;
    //bin::IOManager iom(2, false);
    bin::IOManager iom(1, false);
    iom.schedule(&test_fiber);
}

bin::Timer::ptr s_timer;
void test_timer(){
    bin::IOManager iom(1);
    s_timer = iom.addTimer(1000, [](){
        static int i = 0;
        BIN_LOG_INFO(g_logger) << "hello timer i=" << i;
        if(++i == 3){
            s_timer->reset(2000, true);
            //s_timer->cancel();
        }
    }, true);
}

int main(int argc, char** argv){
    //test1();
    test_timer();
    return 0;
}
#endif
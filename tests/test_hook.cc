// #include "../IOCoroutineScheduler/hook.h"
// #include "IOCoroutineScheduler/log.h"
// #include "IOCoroutineScheduler/iomanager.h"
#include "IOCoroutineScheduler/bin.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

bin::Logger::ptr g_logger = BIN_LOG_ROOT();


/*//block1: 测试休眠
思路：
    正常来讲，系统调用的休眠函数会将整个线程挂起。而此时我们已经把休眠函数HOOK为我们自己实现的针对协程的的休眠，只会将协程挂起，而不会将线程也挂起。
内容：
    构建一个IO调度器IOManager放入两个协程去运行，一个协程运行3s，另一个协程运行2s。
    如果在3s内两个协程都能运行完毕，说明休眠函数HOOK成功。否则，线程级别休眠，应该是5s才能运行完毕。
*/
void test_sleep(){
    bin::IOManager iom(1);
    iom.schedule([](){
        sleep(5);
        BIN_LOG_INFO(g_logger) << "sleep 2";
    });

    iom.schedule([](){
        sleep(5);
        BIN_LOG_INFO(g_logger) << "sleep 3";
    });

    BIN_LOG_INFO(g_logger) << "test_sleep";
}


void test_sock(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "39.156.66.10", &addr.sin_addr.s_addr);

    BIN_LOG_INFO(g_logger) << "begin connect";
    int rt = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    BIN_LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;

    if(rt){
        return;
    }

    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sock, data, sizeof(data), 0);
    BIN_LOG_INFO(g_logger) << "send rt=" << rt << " errno=" << errno;

    if(rt <= 0)
        return;

    //不用char* 在栈上开辟空间，因为协程的栈分配的不会太大，空间比较大，协程一多内存空间消耗的很快，切换比较慢
    std::string buff;
    buff.resize(4096);

    rt = recv(sock, &buff[0], buff.size(), 0);
    BIN_LOG_INFO(g_logger) << "recv rt=" << rt << " errno=" << errno;

    if(rt <= 0)
        return;

    buff.resize(rt);
    BIN_LOG_INFO(g_logger) << buff;
}


int main(){
    test_sleep();

    //bin::IOManager iom;
    //iom.schedule(test_sock);
    return 0;
}
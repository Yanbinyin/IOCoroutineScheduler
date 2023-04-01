#include "IOCoroutineScheduler/tcp_server.h"
#include "IOCoroutineScheduler/log.h"
#include "IOCoroutineScheduler/iomanager.h"
#include "IOCoroutineScheduler/bytearray.h"
#include "IOCoroutineScheduler/address.h"

static bin::Logger::ptr g_logger = BIN_LOG_ROOT();

/*目的：构建一个相对标准的对这一套服务器框架接口使用的小实例，以加强对接口使用的理解以及排错

使用：
1. 继承tcp_server.cpp封装好的TCP服务器，重写虚函数handleClient()执行本次服务器中我们希望执行的服务：接收来自客户端的消息
    并且缓存在内存池ByteArray类中，又从内存池中读出数据，并且打印验证。
2. 写一个run()方法，在其中构建一个EchoServer对象，作为服务器一直被调度。绑定任意地址0.0.0.0+8888端口，作为服务器的对外通信网络地址。
    开启服务器运行，开始监听、接收新客户端的连接请求。
    然后通过另一个终端连接，该程序收集telnet输入的所有命令
         telnet 127.0.0.1 8888
*/
/*
shell输入 ../echo_server -t 运行应用
测试方法1：
    新开一个shell 输入starbin@cmp-n:~$ telnet 127.0.0.1 8888
    Trying 127.0.0.1...
    Connected to 127.0.0.1.
    Escape character is '^]'.
    hello

    原来的shell会有响应
测试方法2：
    web浏览器地址栏输入
        ip：8888进行访问
*/

//block: 类部分
class EchoServer : public bin::TcpServer {
public:
    EchoServer(int type);
    void handleClient(bin::Socket::ptr client) override;  //重写TCPServer类的虚函数 处理已经连接的客户端  完成数据交互通信

private:
    int m_type = 0; //标识输出的内容是二进制还是文本，二进制的toString转成HEX进制输出
};


EchoServer::EchoServer(int type)
    :m_type(type){
}

void EchoServer::handleClient(bin::Socket::ptr client){
    BIN_LOG_INFO(g_logger) << "handleClient " << *client;   
    bin::ByteArray::ptr ba(new bin::ByteArray);
    while(true){
        ba->clear();
        std::vector<iovec> iovs;
        ba->getWriteBuffers(iovs, 1024);

        int rt = client->recv(&iovs[0], iovs.size());
        if(rt == 0){
            BIN_LOG_INFO(g_logger) << "client close: " << *client;
            break;
        } else if(rt < 0){
            BIN_LOG_INFO(g_logger) << "client error rt=" << rt << " errno=" << errno << " errstr=" << strerror(errno);
            break;
        }
        ba->setPosition(ba->getPosition() + rt);    //getWriteBuffers不会修改position的位置，set一下
        ba->setPosition(0);     //set 0 是为了toString
        //BIN_LOG_INFO(g_logger) << "recv rt=" << rt << " data=" << std::string((char*)iovs[0].iov_base, rt);
        if(m_type == 1){//text 
            std::cout << ba->toString();// << std::endl;
        } else {
            std::cout << ba->toHexString();// << std::endl;
        }
        std::cout.flush();
    }
}



int type = 1;

void run(){
    BIN_LOG_INFO(g_logger) << "server type=" << type;
    EchoServer::ptr es(new EchoServer(type));
    auto addr = bin::Address::LookupAny("0.0.0.0:8888");
    while(!es->bind(addr)){
        sleep(2);
    }
    es->start();
}


//argc表示参数的个数(至少为1，argv[0]=调用函数时的命令)，argv后面的参数是其他命令
int main(int argc, char** argv){
    BIN_LOG_INFO(g_logger) << argc << " " << argv[0];

    if(argc < 2){
        BIN_LOG_INFO(g_logger) << "used as[" << argv[0] << " -t] or [" << argv[0] << " -b]";
        return 0;
    }

    //-b表示二进制输出
    if(!strcmp(argv[1], "-b")){
        type = 2;
    }

    bin::IOManager iom(2);
    iom.schedule(run);
    return 0;
}

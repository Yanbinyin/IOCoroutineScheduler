#include "tcp_server.h"
#include "config.h"
#include "log.h"

namespace sylar {

    static sylar::ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout =
            sylar::Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2),"tcp server read timeout");

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    TcpServer::TcpServer(sylar::IOManager* worker, sylar::IOManager* io_worker, sylar::IOManager* accept_worker)
        :m_worker(worker)
        ,m_ioWorker(io_worker)
        ,m_acceptWorker(accept_worker)
        ,m_recvTimeout(g_tcp_server_read_timeout->getValue())
        ,m_name("sylar/1.0.0")  //服务器的版本号
        ,m_isStop(true){
    }

    TcpServer::~TcpServer(){
        //关闭所有的监听套接字
        for(auto& i : m_listenSocks)
            i->close();
        m_listenSocks.clear();
    }



    //支持一个地址bind  直接复用多个地址的情况 只是容器里只有一个地址
    bool TcpServer::bind(sylar::Address::ptr addr, bool ssl){
        std::vector<Address::ptr> addrs;
        std::vector<Address::ptr> fails;
        addrs.push_back(addr);
        return bind(addrs, fails, ssl);
    }

    bool TcpServer::bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails, bool ssl){
        m_ssl = ssl;
        for(auto& x : addrs){
            Socket::ptr sock = ssl ? SSLSocket::CreateTCP(x) : Socket::CreateTCP(x);
            if(!sock->bind(x)){ //bind绑定地址
                SYLAR_LOG_ERROR(g_logger) << "bind fail errno=" << errno << " errstr=" << strerror(errno) << " addr=[" << x->toString() << "]";
                fails.push_back(x);
                continue;
            }
            if(!sock->listen()){  //listen监听地址
                SYLAR_LOG_ERROR(g_logger) << "listen fail errno=" << errno << " errstr=" << strerror(errno) << " addr=[" << x->toString() << "]";
                fails.push_back(x); //成功监听
                continue;
            }
            m_listenSocks.push_back(sock); //成功监听的储存下来
        }

         //如果存在监听失败的套接字 要将成功监听那部分清除
        if(!fails.empty()){
            m_listenSocks.clear();
            return false;
        }

        //利用重载<<符号 打印一下Socket的内容
        for(auto& i : m_listenSocks) 
            SYLAR_LOG_INFO(g_logger) << "type=" << m_type << " name=" << m_name << " ssl=" << m_ssl << " server bind success: " << *i;
        
        return true;
    }

    bool TcpServer::start(){
        if(!m_isStop){
            return true;
        }
        m_isStop = false;
        //开启服务器，给每个监听套接字分配一个执行函数startAccept()去监测客户端的新连接，将其作为任务加入到IO调度器m_acceptWorker去进行调度管理
        for(auto& sock : m_listenSocks)
            m_acceptWorker->schedule(std::bind(&TcpServer::startAccept, shared_from_this(), sock));
        
        return true;
    }

    //停止服务器，通过往IO调度器m_acceptWorker中添加任务的形式，在该匿名任务中唤醒所有监听线程并且让协程调度停止、所有线程退出
    void TcpServer::stop(){
        m_isStop = true;
        auto self = shared_from_this();
        m_acceptWorker->schedule([this, self](){
            //唤醒所有线程 进行退出。因为是accept不取消事件，不会唤醒
            for(auto& sock : m_listenSocks){
                sock->cancelAll();
                sock->close();
            }
            m_listenSocks.clear();
        });
    }



    //真正负责客户端连接工作的处理函数，给每个通信套接字分配一个执行函数handleClient()去完成服务器和客户端的数据交互，将其作为任务加入到IO调度器m_ioWorker去进行调度管理
    void TcpServer::startAccept(Socket::ptr sock){
        //循环 只要不停止就要一直去accept客户端
        while(!m_isStop){
            //使用Socket套接字类封装的accept（被实施了HOOK处理，将执行自己封装的调用行为），由于原系统调用::accept必定造成阻塞，为了提高处理效率
            //因此会将该函数使用之前开发的hook.cpp:do_io()利用协程切入/切出进行一个异步回调（当真的有连接到达时候才来执行相应的操作，而不必一直阻塞等待），
            Socket::ptr client = sock->accept();
            if(client){
                client->setRecvTimeout(m_recvTimeout); //给每一通信套接字设置读超时
                
                //将通信套接字加入线程池管理
                //bind()函数适配器传入shared_from_this（this指针封装为智能指针形式）是为了增加对该TcpServer对象的引用，防止handleClient结束之前tcpserver意外析构造成毁灭性错误
                m_ioWorker->schedule( std::bind(&TcpServer::handleClient, shared_from_this(), client) );
            }else{
                SYLAR_LOG_ERROR(g_logger) << "accept errno=" << errno << " errstr=" << strerror(errno);
            }
        }
    }

    //真正负责服务器和客户端之间数据交互的处理函数。
    //假如我们要做即时通讯服务器，那么聊天信息的转发和交换就在该函数中完成；假如做游戏服务器，那么游戏角色的相关信息更新在该函数中完成······
    void TcpServer::handleClient(Socket::ptr client){
        SYLAR_LOG_INFO(g_logger) << "handleClient: " << *client;
        /*需要进行的操作*/
    }



    bool TcpServer::loadCertificates(const std::string& cert_file, const std::string& key_file){
        for(auto& i : m_listenSocks){
            auto ssl_socket = std::dynamic_pointer_cast<SSLSocket>(i);
            if(ssl_socket){
                if(!ssl_socket->loadCertificates(cert_file, key_file)){
                    return false;
                }
            }
        }
        return true;
    }



    void TcpServer::setConf(const TcpServerConf& v){
        m_conf.reset(new TcpServerConf(v));
    }

    std::string TcpServer::toString(const std::string& prefix){
        std::stringstream ss;
        ss << prefix << "[type=" << m_type
        << " name=" << m_name << " ssl=" << m_ssl
        << " worker=" << (m_worker ? m_worker->getName() : "")
        << " accept=" << (m_acceptWorker ? m_acceptWorker->getName() : "")
        << " recv_timeout=" << m_recvTimeout << "]" << std::endl;
        std::string pfx = prefix.empty() ? "    " : prefix;
        for(auto& i : m_listenSocks){
            ss << pfx << pfx << *i << std::endl;
        }
        return ss.str();
    }

}

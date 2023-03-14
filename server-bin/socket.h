//Socket封装

#ifndef __SYLAR_SOCKET_H__
#define __SYLAR_SOCKET_H__

#include <memory>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "address.h"
#include "noncopyable.h"

namespace sylar{
    /*
    //skip: 那些API   
            语雀的socket的最后
            SSLSocket
    */
    //socket封装类
    class Socket : public std::enable_shared_from_this<Socket>, Noncopyable{
    public:
        typedef std::shared_ptr<Socket> ptr;
        typedef std::weak_ptr<Socket> weak_ptr;

        //Socket类型
        enum Type{
            TCP = SOCK_STREAM, //TCP类型
            UDP = SOCK_DGRAM   //UDP类型
        };

        //Socket协议簇
        enum Family{
            IPv4 = AF_INET,    //IPv4 socket
            IPv6 = AF_INET6,   //IPv6 socket
            UNIX = AF_UNIX,    //Unix socket
        };

        static Socket::ptr CreateTCP(sylar::Address::ptr address);  //根据address的sa_family类型，创建IPv4/6 UNIX domain的TCP套接字对象
        static Socket::ptr CreateUDP(sylar::Address::ptr address);  //根据address的sa_family类型，创建IPv4/6 UNIX domain的UDP套接字对象
        static Socket::ptr CreateTCPSocket();        //默认创建TCP IPv4 套接字
        static Socket::ptr CreateUDPSocket();        //默认创建UDP IPv4 套接字
        static Socket::ptr CreateTCPSocket6();       //默认创建TCP IPv6 套接字
        static Socket::ptr CreateUDPSocket6();       //默认创建UDP IPv6 套接字
        static Socket::ptr CreateUnixTCPSocket();    //默认创建TCP UNIX域 套接字
        static Socket::ptr CreateUnixUDPSocket();    //默认创建UDP UNIX域 套接字

        Socket(int family, int type, int protocol = 0); //family: 协议簇  type: 类型  protocol: 协议
        virtual ~Socket();

        //sock初始化init()的时候，将sock的fd加入到FdManager的管理中
        int64_t getSendTimeout();       //获取fd句柄上的发送超时时间(毫秒)
        void setSendTimeout(int64_t v); //设置fd句柄上的发送超时时间(毫秒)
        int64_t getRecvTimeout();       //获取fd句柄上的接受超时时间(毫秒)
        void setRecvTimeout(int64_t v); //设置fd句柄上的接受超时时间(毫秒)

        //获取套接字配置信息 getsockopt
        bool getOption(int level, int option, void* result, socklen_t* len);
        //获取sockopt模板 getsockopt
        template<class T>
        bool getOption(int level, int option, T& result){
            socklen_t length = sizeof(T);
            return getOption(level, option, &result, &length);
        }
        
        //设置套接字配置信息 setsockopt
        //level: 在哪一个层级句柄设置 option: 设置的具体配置 result 设置的结果 len 参数大小(result=sizeof(len))
        bool setOption(int level, int option, const void* result, socklen_t len);
        //设置sockopt模板 setsockopt
        template<class T>
        bool setOption(int level, int option, const T& value){
            return setOption(level, option, &value, sizeof(T));
        }

        //block: socket相关的API
        virtual Socket::ptr accept();   //接收connect链接,成功返回新连接的socket,失败返回nullptr
        virtual bool bind(const Address::ptr addr); //绑定地址，返回是否绑定成功  addr: 地址
        virtual bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);    //连接地址 addr: 目标地址 timeout_ms: 超时时间(毫秒)
        virtual bool reconnect(uint64_t timeout_ms = -1);
        virtual bool listen(int backlog = SOMAXCONN);   //监听socket，返回监听是否成功，必须先 bind 成功  backlog: 未完成连接队列的最大长度
        virtual bool close();   //关闭socket

        //block: 发送接收接收的API
        //buffer：待发送数据的内存；    length：待发送数据的长度；  to：发送的目标地址；    flags：标志字；
        //返回值：>0 发送成功对应大小的数据；   =0 socket被关闭；   <0 socket出错
        virtual int send(const void* buffer, size_t length, int flags = 0);
        virtual int send(const iovec* buffers, size_t length, int flags = 0);
        virtual int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);
        virtual int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0);
        virtual int recv(void* buffer, size_t length, int flags = 0);
        virtual int recv(iovec* buffers, size_t length, int flags = 0);
        virtual int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);
        virtual int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0);

        //block：辅助函数
        Address::ptr getRemoteAddress();            //获取远端地址
        Address::ptr getLocalAddress();             //获取本地地址，如果还没有初始化一个

        int getFamily() const{ return m_family;}           //获取协议簇
        int getType() const{ return m_type;}               //获取类型
        int getProtocol() const{ return m_protocol;}       //获取协议
        bool isConnected() const{ return m_isConnected;}   //返回是否连接
        int getSocket() const{ return m_sockfd;}           //返回socket句柄
        bool isValid() const{ return m_sockfd != -1; };    //是否有效(m_sock != -1)，初始化的东西是否已经做好
        int getError();                                     //返回Socket错误
        
        virtual std::ostream& dump(std::ostream& os) const; //输出信息到流中
        virtual std::string toString() const;

        bool cancelRead();      //取消读
        bool cancelWrite();     //取消写
        bool cancelAccept();    //取消accept
        bool cancelAll();       //取消所有事件

    protected:
        //初始化socket      调用setoption() <初始化> 套接字句柄m_sockfd
        void initSock();
        //创建初始化socket  调用之前hook好的socket()创建m_sockfd并initSock()初始化
        void newSock();
        //初始化sock        将sockfd赋给m_sockfd，并初始化这个sockfd(类的其他成员)，initSock()、get…… 
        virtual bool init(int sockfd);
        /*
            bind       accpet
              |          |
              |          |
             \ /        \ /
            newsock   newsock()
               \        / 
                \      /
                  init
        */

    protected:
        int m_sockfd;    //socket句柄
        int m_family;    //协议簇
        int m_type;      //类型
        int m_protocol;  //协议
        bool m_isConnected;             //是否连接
        Address::ptr m_localAddress;    //本地地址
        Address::ptr m_remoteAddress;   //远端地址
    };





    class SSLSocket : public Socket{
    public:
        typedef std::shared_ptr<SSLSocket> ptr;

        static SSLSocket::ptr CreateTCP(sylar::Address::ptr address);
        static SSLSocket::ptr CreateTCPSocket();
        static SSLSocket::ptr CreateTCPSocket6();

        SSLSocket(int family, int type, int protocol = 0);
        virtual Socket::ptr accept() override;
        virtual bool bind(const Address::ptr addr) override;
        virtual bool connect(const Address::ptr addr, uint64_t timeout_ms = -1) override;
        virtual bool listen(int backlog = SOMAXCONN) override;
        virtual bool close() override;
        virtual int send(const void* buffer, size_t length, int flags = 0) override;
        virtual int send(const iovec* buffers, size_t length, int flags = 0) override;
        virtual int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0) override;
        virtual int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0) override;
        virtual int recv(void* buffer, size_t length, int flags = 0) override;
        virtual int recv(iovec* buffers, size_t length, int flags = 0) override;
        virtual int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0) override;
        virtual int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0) override;

        bool loadCertificates(const std::string& cert_file, const std::string& key_file);
        virtual std::ostream& dump(std::ostream& os) const override;
    protected:
        virtual bool init(int sock) override;
    private:
        std::shared_ptr<SSL_CTX> m_ctx;
        std::shared_ptr<SSL> m_ssl;
    };



    /**
     * @brief 流式输出socket
     * @param[in, out] os 输出流
     * @param[in] sock Socket类
     */
    std::ostream& operator<<(std::ostream& os, const Socket& sock);

}

#endif

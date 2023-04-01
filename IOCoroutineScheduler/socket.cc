#include "socket.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "log.h"
#include "macro.h"
#include "hook.h"
#include <limits.h>

namespace bin{

    static bin::Logger::ptr g_logger = BIN_LOG_NAME("system");

    //创建TCP/UDP
    Socket::ptr Socket::CreateTCP(bin::Address::ptr address){
        Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDP(bin::Address::ptr address){
        Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
        //hack: 为什么不统一，语雀可以做到十分统一协调，换句话说，为什么UDP就需要newSock
        //毕竟无论是TCP or UDP，流socket的运作都需要bind，自定义的bind里面调用了newSock
        sock->newSock();
        sock->m_isConnected = true;
        return sock;
    }

    Socket::ptr Socket::CreateTCPSocket(){
        Socket::ptr sock(new Socket(IPv4, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDPSocket(){
        Socket::ptr sock(new Socket(IPv4, UDP, 0));
        sock->newSock();
        sock->m_isConnected = true;
        return sock;
    }

    Socket::ptr Socket::CreateTCPSocket6(){
        Socket::ptr sock(new Socket(IPv6, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDPSocket6(){
        Socket::ptr sock(new Socket(IPv6, UDP, 0));
        sock->newSock();
        sock->m_isConnected = true;
        return sock;
    }

    Socket::ptr Socket::CreateUnixTCPSocket(){
        Socket::ptr sock(new Socket(UNIX, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUnixUDPSocket(){
        Socket::ptr sock(new Socket(UNIX, UDP, 0));
        return sock;
    }



    //构造析构函数
    Socket::Socket(int family, int type, int protocol)
        :m_sockfd(-1)
        ,m_family(family)
        ,m_type(type)
        ,m_protocol(protocol)
        ,m_isConnected(false){
    }

    Socket::~Socket(){
        close(); //关闭socket
    }



    int64_t Socket::getSendTimeout(){
        FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sockfd);
        if(ctx)
            return ctx->getTimeout(SO_SNDTIMEO);
        return -1;
    }

    void Socket::setSendTimeout(int64_t v){
        struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
        setOption(SOL_SOCKET, SO_SNDTIMEO, tv); //设置套接字属性
    }
    
    int64_t Socket::getRecvTimeout(){
        FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sockfd);
        if(ctx)
            return ctx->getTimeout(SO_RCVTIMEO);
        return -1;
    }

    void Socket::setRecvTimeout(int64_t v){
        struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
        setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
    }




    bool Socket::getOption(int level, int option, void* result, socklen_t* len){
        int ret = getsockopt(m_sockfd, level, option, result, (socklen_t*)len);
        if(ret){
            BIN_LOG_DEBUG(g_logger) << "getOption sock=" << m_sockfd << " level=" << level << " option=" << option << " errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }
        return true;
    }

    bool Socket::setOption(int level, int option, const void* result, socklen_t len){
        int ret =setsockopt(m_sockfd, level, option, result, (socklen_t)len);
        if(ret){
            BIN_LOG_DEBUG(g_logger) << "setOption sock=" << m_sockfd << " level=" << level << " option=" << option << " errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }
        return true;
    }



    //block: socket相关的API
    Socket::ptr Socket::accept(){
        Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
        int newsock = ::accept(m_sockfd, nullptr, nullptr);
        if(newsock == -1){
            BIN_LOG_ERROR(g_logger) << "accept(" << m_sockfd << ") errno=" << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        if(sock->init(newsock)){
            return sock;
        }
        return nullptr;
    }

    bool Socket::bind(const Address::ptr addr){
        //m_localAddress = addr;
        //如果套接字无效
        if(!isValid()){
            newSock();  //创建新的套接字
            if(BIN_UNLIKELY(!isValid()))
                return false;
        }

        //socket类型和address类型不符
        if(BIN_UNLIKELY(addr->getFamily() != m_family)){
            BIN_LOG_ERROR(g_logger) << "bind sock.family(" << m_family << ") addr.family(" << addr->getFamily() << ") not equal, addr=" << addr->toString();
            return false;
        }

        //skip
        UnixAddress::ptr uaddr = std::dynamic_pointer_cast<UnixAddress>(addr);
        if(uaddr){
            Socket::ptr sock = Socket::CreateUnixTCPSocket();
            if(sock->connect(uaddr))
                return false;
            else
                bin::FSUtil::Unlink(uaddr->getPath(), true);

        }

        //bind 没有被HOOK
        int ret = ::bind(m_sockfd, addr->getAddr(), addr->getAddrLen());
        if(ret < 0){
            BIN_LOG_ERROR(g_logger) << "bind error errrno=" << errno << " errstr=" << strerror(errno);
            return false;
        }
        getLocalAddress();
        return true;
    }

    bool Socket::reconnect(uint64_t timeout_ms){
        if(!m_remoteAddress){
            BIN_LOG_ERROR(g_logger) << "reconnect m_remoteAddress is null";
            return false;
        }
        m_localAddress.reset();
        return connect(m_remoteAddress, timeout_ms);
    }

    bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms){
        m_remoteAddress = addr;
        if(!isValid()){
            newSock();
            if(BIN_UNLIKELY(!isValid()))
                return false;
        }

        //socket类型和address类型不符
        if(BIN_UNLIKELY(addr->getFamily() != m_family)){
            BIN_LOG_ERROR(g_logger) << "connect sock.family(" << m_family << ") addr.family(" << addr->getFamily()
                << ") not equal, addr=" << addr->toString();
            return false;
        }

        if(timeout_ms == (uint64_t)-1){
            //会使用默认超时时间
            int ret = ::connect(m_sockfd, addr->getAddr(), addr->getAddrLen());
            if(ret < 0){
                BIN_LOG_ERROR(g_logger) << "sock=" << m_sockfd << " connect(" << addr->toString()
                    << ") error errno=" << errno << " errstr=" << strerror(errno);
                close();
                return false;
            }
        }else{//使用超时connect
            int ret = ::connect_with_timeout(m_sockfd, addr->getAddr(), addr->getAddrLen(), timeout_ms);
            if(ret < 0){
                BIN_LOG_ERROR(g_logger) << "sock=" << m_sockfd << " connect(" << addr->toString()
                    << ") timeout=" << timeout_ms << " error errno=" << errno << " errstr=" << strerror(errno);
                close();
                return false;
            }
        }
        m_isConnected = true;
        getRemoteAddress(); //获取远端地址
        getLocalAddress();  //获取本地地址
        return true;
    }

    bool Socket::listen(int backlog){
        if(!isValid()){
            BIN_LOG_ERROR(g_logger) << "listen error sock=-1";
            return false;
        }
        if(::listen(m_sockfd, backlog)){
            BIN_LOG_ERROR(g_logger) << "listen error errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }
        return true;
    }

    bool Socket::close(){
        if(!m_isConnected && m_sockfd == -1){
            return true;
        }
        m_isConnected = false;
        if(m_sockfd != -1){
            ::close(m_sockfd);
            m_sockfd = -1;
        }
        return false;
    }



    //block: 发送接收接收的API
    int Socket::send(const void* buffer, size_t length, int flags){
        if(isConnected())
            return ::send(m_sockfd, buffer, length, flags);
        return -1;
    }

    int Socket::send(const iovec* buffers, size_t length, int flags){
        if(isConnected()){
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            return ::sendmsg(m_sockfd, &msg, flags);
        }
        return -1;
    }

    int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags){
        if(isConnected())
            return ::sendto(m_sockfd, buffer, length, flags, to->getAddr(), to->getAddrLen());
        return -1;
    }

    int Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags){
        if(isConnected()){
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            msg.msg_name = to->getAddr();
            msg.msg_namelen = to->getAddrLen();
            return ::sendmsg(m_sockfd, &msg, flags);
        }
        return -1;
    }

    int Socket::recv(void* buffer, size_t length, int flags){
        if(isConnected())
            return ::recv(m_sockfd, buffer, length, flags);
        return -1;
    }

    int Socket::recv(iovec* buffers, size_t length, int flags){
        if(isConnected()){
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            return ::recvmsg(m_sockfd, &msg, flags);
        }
        return -1;
    }

    int Socket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags){
        if(isConnected()){
            socklen_t len = from->getAddrLen();
            return ::recvfrom(m_sockfd, buffer, length, flags, from->getAddr(), &len);
        }
        return -1;
    }

    int Socket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags){
        if(isConnected()){
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            msg.msg_name = from->getAddr();
            msg.msg_namelen = from->getAddrLen();
            return ::recvmsg(m_sockfd, &msg, flags);
        }
        return -1;
    }



    Address::ptr Socket::getRemoteAddress(){
        if(m_remoteAddress)
            return m_remoteAddress;

        Address::ptr result;
        switch(m_family){
            case AF_INET:
                result.reset(new IPv4Address());
                break;
            case AF_INET6:
                result.reset(new IPv6Address());
                break;
            case AF_UNIX:
                result.reset(new UnixAddress());
                break;
            default:
                result.reset(new UnknownAddress(m_family));
                break;
        }
        socklen_t addrlen = result->getAddrLen();
        if(getpeername(m_sockfd, result->getAddr(), &addrlen)){
            //BIN_LOG_ERROR(g_logger) << "getpeername error sock=" << m_sock << " errno=" << errno << " errstr=" << strerror(errno);
            return Address::ptr(new UnknownAddress(m_family));
        }
        if(m_family == AF_UNIX){
            UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
            addr->setAddrLen(addrlen);
        }
        m_remoteAddress = result;
        return m_remoteAddress;
    }

    Address::ptr Socket::getLocalAddress(){
        if(m_localAddress)
            return m_localAddress;

        Address::ptr result;
        switch(m_family){
            case AF_INET:
                result.reset(new IPv4Address());
                break;
            case AF_INET6:
                result.reset(new IPv6Address());
                break;
            case AF_UNIX:
                result.reset(new UnixAddress());
                break;
            default:
                result.reset(new UnknownAddress(m_family));
                break;
        }
        socklen_t addrlen = result->getAddrLen();
        int ret = getsockname(m_sockfd, result->getAddr(), &addrlen);
        if(ret){
            BIN_LOG_ERROR(g_logger) << "getsockname error sock=" << m_sockfd << " errno=" << errno << " errstr=" << strerror(errno);
            return Address::ptr(new UnknownAddress(m_family));
        }
        if(m_family == AF_UNIX){
            UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
            addr->setAddrLen(addrlen);
        }
        m_localAddress = result;
        return m_localAddress;
    }


    int Socket::getError(){
        int error = 0;
        socklen_t len = sizeof(error);
        if(!getOption(SOL_SOCKET, SO_ERROR, &error, &len)){
            error = errno;
        }
        return error;
    }


    std::ostream& Socket::dump(std::ostream& os) const{
        os << "[Socket sockfd=" << m_sockfd
        << " is_connected=" << m_isConnected
        << " family=" << m_family
        << " type=" << m_type
        << " protocol=" << m_protocol;
        if(m_localAddress){
            os << " local_address=" << m_localAddress->toString();
        }
        if(m_remoteAddress){
            os << " remote_address=" << m_remoteAddress->toString();
        }
        os << "]";
        return os;
    }

    std::string Socket::toString() const{
        std::stringstream ss;
        dump(ss);
        return ss.str();
    }


    bool Socket::cancelRead(){
        return IOManager::GetThis()->cancelEvent(m_sockfd, bin::IOManager::READ);
    }

    bool Socket::cancelWrite(){
        return IOManager::GetThis()->cancelEvent(m_sockfd, bin::IOManager::WRITE);
    }

    bool Socket::cancelAccept(){
        return IOManager::GetThis()->cancelEvent(m_sockfd, bin::IOManager::READ);
    }

    bool Socket::cancelAll(){
        return IOManager::GetThis()->cancelAll(m_sockfd);
    }



    //设置套接字为端口复用，如果是TCP，还设置为非阻塞
    void Socket::initSock(){
        int val = 1;
        setOption(SOL_SOCKET, SO_REUSEADDR, val);   //P1051，服务器一般都需要设置SO_REUSEADDR
        //禁用TCP套接字的 nagle算法
        if(m_type == SOCK_STREAM)
            setOption(IPPROTO_TCP, TCP_NODELAY, val);
    }

    //创建一个新的套接字句柄。将之前hook好的 API 简单封装即可
    void Socket::newSock(){
        m_sockfd = socket(m_family, m_type, m_protocol);
        if(BIN_LIKELY(m_sockfd != -1)){
            initSock();
        }else{
            BIN_LOG_ERROR(g_logger) << "socket(" << m_family << ", " << m_type << ", " << m_protocol << ") errno=" << errno << " errstr=" << strerror(errno);
        }
    }

    //初始化一个新的sock句柄
    bool Socket::init(int sockfd){
        //新的fd加入到FdManager的管理中
        FdCtx::ptr ctx = FdMgr::GetInstance()->get(sockfd);
        //fd为socket 并且没被关闭 对其进行初始化
        if(ctx && ctx->isSocket() && !ctx->isClose()){
            m_sockfd = sockfd;
            m_isConnected = true;
            initSock();
            getLocalAddress();
            getRemoteAddress();
            return true;
        }
        return false;
    }


    

    namespace{
        struct _SSLInit{
            _SSLInit(){
                SSL_library_init();
                SSL_load_error_strings();
                OpenSSL_add_all_algorithms();
            }
        };

        static _SSLInit s_init;
    }



    SSLSocket::SSLSocket(int family, int type, int protocol)
        :Socket(family, type, protocol){
    }

    Socket::ptr SSLSocket::accept(){
        SSLSocket::ptr sock(new SSLSocket(m_family, m_type, m_protocol));
        int newsock = ::accept(m_sockfd, nullptr, nullptr);
        if(newsock == -1){
            BIN_LOG_ERROR(g_logger) << "accept(" << m_sockfd << ") errno="
                << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        sock->m_ctx = m_ctx;
        if(sock->init(newsock)){
            return sock;
        }
        return nullptr;
    }

    bool SSLSocket::bind(const Address::ptr addr){
        return Socket::bind(addr);
    }

    bool SSLSocket::connect(const Address::ptr addr, uint64_t timeout_ms){
        bool v = Socket::connect(addr, timeout_ms);
        if(v){
            m_ctx.reset(SSL_CTX_new(SSLv23_client_method()), SSL_CTX_free);
            m_ssl.reset(SSL_new(m_ctx.get()),  SSL_free);
            SSL_set_fd(m_ssl.get(), m_sockfd);
            v = (SSL_connect(m_ssl.get()) == 1);
        }
        return v;
    }

    bool SSLSocket::listen(int backlog){
        return Socket::listen(backlog);
    }

    bool SSLSocket::close(){
        return Socket::close();
    }

    int SSLSocket::send(const void* buffer, size_t length, int flags){
        if(m_ssl){
            return SSL_write(m_ssl.get(), buffer, length);
        }
        return -1;
    }

    int SSLSocket::send(const iovec* buffers, size_t length, int flags){
        if(!m_ssl){
            return -1;
        }
        int total = 0;
        for(size_t i = 0; i < length; ++i){
            int tmp = SSL_write(m_ssl.get(), buffers[i].iov_base, buffers[i].iov_len);
            if(tmp <= 0){
                return tmp;
            }
            total += tmp;
            if(tmp != (int)buffers[i].iov_len){
                break;
            }
        }
        return total;
    }

    int SSLSocket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags){
        BIN_ASSERT(false);
        return -1;
    }

    int SSLSocket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags){
        BIN_ASSERT(false);
        return -1;
    }

    int SSLSocket::recv(void* buffer, size_t length, int flags){
        if(m_ssl){
            return SSL_read(m_ssl.get(), buffer, length);
        }
        return -1;
    }

    int SSLSocket::recv(iovec* buffers, size_t length, int flags){
        if(!m_ssl){
            return -1;
        }
        int total = 0;
        for(size_t i = 0; i < length; ++i){
            int tmp = SSL_read(m_ssl.get(), buffers[i].iov_base, buffers[i].iov_len);
            if(tmp <= 0){
                return tmp;
            }
            total += tmp;
            if(tmp != (int)buffers[i].iov_len){
                break;
            }
        }
        return total;
    }

    int SSLSocket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags){
        BIN_ASSERT(false);
        return -1;
    }

    int SSLSocket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags){
        BIN_ASSERT(false);
        return -1;
    }

    bool SSLSocket::init(int sock){
        bool v = Socket::init(sock);
        if(v){
            m_ssl.reset(SSL_new(m_ctx.get()),  SSL_free);
            SSL_set_fd(m_ssl.get(), m_sockfd);
            v = (SSL_accept(m_ssl.get()) == 1);
        }
        return v;
    }

    bool SSLSocket::loadCertificates(const std::string& cert_file, const std::string& key_file){
        m_ctx.reset(SSL_CTX_new(SSLv23_server_method()), SSL_CTX_free);
        if(SSL_CTX_use_certificate_chain_file(m_ctx.get(), cert_file.c_str()) != 1){
            BIN_LOG_ERROR(g_logger) << "SSL_CTX_use_certificate_chain_file("
                << cert_file << ") error";
            return false;
        }
        if(SSL_CTX_use_PrivateKey_file(m_ctx.get(), key_file.c_str(), SSL_FILETYPE_PEM) != 1){
            BIN_LOG_ERROR(g_logger) << "SSL_CTX_use_PrivateKey_file("
                << key_file << ") error";
            return false;
        }
        if(SSL_CTX_check_private_key(m_ctx.get()) != 1){
            BIN_LOG_ERROR(g_logger) << "SSL_CTX_check_private_key cert_file="
                << cert_file << " key_file=" << key_file;
            return false;
        }
        return true;
    }

    SSLSocket::ptr SSLSocket::CreateTCP(bin::Address::ptr address){
        SSLSocket::ptr sock(new SSLSocket(address->getFamily(), TCP, 0));
        return sock;
    }

    SSLSocket::ptr SSLSocket::CreateTCPSocket(){
        SSLSocket::ptr sock(new SSLSocket(IPv4, TCP, 0));
        return sock;
    }

    SSLSocket::ptr SSLSocket::CreateTCPSocket6(){
        SSLSocket::ptr sock(new SSLSocket(IPv6, TCP, 0));
        return sock;
    }

    std::ostream& SSLSocket::dump(std::ostream& os) const{
        os << "[SSLSocket sock=" << m_sockfd
        << " is_connected=" << m_isConnected
        << " family=" << m_family
        << " type=" << m_type
        << " protocol=" << m_protocol;
        if(m_localAddress){
            os << " local_address=" << m_localAddress->toString();
        }
        if(m_remoteAddress){
            os << " remote_address=" << m_remoteAddress->toString();
        }
        os << "]";
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const Socket& sock){
        return sock.dump(os);
    }

}

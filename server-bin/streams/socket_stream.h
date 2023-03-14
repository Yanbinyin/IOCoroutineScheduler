//Socket流式接口封装
//socket在socket.h的时候已经封装过了，那是对原始C API的封装，将C封装成类
//socket_stream.h是socket业务级接口封装，逻辑里面都用stream，很少用裸的socket

#ifndef __SYLAR_SOCKET_STREAM_H__
#define __SYLAR_SOCKET_STREAM_H__

#include "server-bin/stream.h"
#include "server-bin/socket.h"
#include "server-bin/mutex.h"
#include "server-bin/iomanager.h"

namespace sylar {

    //Socket流:封装一套和业务联系紧密的socket接口。和之前socket.h/socket.cpp封装C API不同，之前为了避免裸用关于socket的接口
    /*
     * @retval >0 返回实际接收到的数据长度
     * @retval =0 socket被远端关闭
     * @retval <0 socket错误
    */
    class SocketStream : public Stream {
    public:
        typedef std::shared_ptr<SocketStream> ptr;

        SocketStream(Socket::ptr sock, bool owner = true);  //sock Socket类 owner 是否完全控制
        ~SocketStream();    //如果m_owner=true,则close

        virtual int read(void* buffer, size_t length) override;
        virtual int read(ByteArray::ptr ba, size_t length) override;
        virtual int write(const void* buffer, size_t length) override;
        virtual int write(ByteArray::ptr ba, size_t length) override;
        virtual void close() override;  //关闭socket

        Socket::ptr getSocket() const { return m_socket;}   //返回Socket类
        bool isConnected() const;                           //判断套接字是否处于连接状态

        Address::ptr getRemoteAddress();
        Address::ptr getLocalAddress();
        std::string getRemoteAddressString();
        std::string getLocalAddressString();

    protected:
        Socket::ptr m_socket;   //Socket类
        bool m_owner;           //句柄全权管理标志位，即m_socket是否全权交给我管理，析构的时候是否析构m_socket
    };

}

#endif

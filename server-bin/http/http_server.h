//HTTP服务器封装
//利用已经封装好的HttpSession类，搭建一个简单的HTTP交互服务器。
//接收HTTP请求报文，正确解析后，给客户端回发一个简单的响应报文，封装原来的请求报文

#ifndef __SYLAR_HTTP_HTTP_SERVER_H__
#define __SYLAR_HTTP_HTTP_SERVER_H__

#include "server-bin/tcp_server.h"
#include "http_session.h"
#include "servlet.h"

namespace sylar::http {
//namespace http {

    //HTTP服务器类
    class HttpServer : public TcpServer {
    public:
        typedef std::shared_ptr<HttpServer> ptr;

        //keepalive 是否长连接 worker 工作调度器 accept_worker 接收连接调度器
        HttpServer(bool keepalive = false
                ,sylar::IOManager* worker = sylar::IOManager::GetThis()
                ,sylar::IOManager* io_worker = sylar::IOManager::GetThis()
                ,sylar::IOManager* accept_worker = sylar::IOManager::GetThis());

        ServletDispatch::ptr getServletDispatch() const { return m_dispatch;}   //获取ServletDispatch
        void setServletDispatch(ServletDispatch::ptr v){ m_dispatch = v;}      //设置ServletDispatch

        virtual void setName(const std::string& v) override;

    protected:
        virtual void handleClient(Socket::ptr client) override; //处理已经连接的客户端  完成数据交互通信

    private:
        bool m_isKeepalive;              //是否支持长连接
        ServletDispatch::ptr m_dispatch; //Servlet分发器
    };
    
    /*//add: 长连接
    http长连接与短连接
        http协议是基于TCP协议的，http长连接即指的是TCP的长连接。TCP协议是一个面向连接、可靠、有着拥塞控制以及流量控制的传输层协议。

    http长连接问题由来
        http是一个无状态的协议，简单点来说，如果同一个浏览器在接连发起2次http请求的话，http无法在第2次请求中识别出这个浏览器刚才交互过，
        所以无状态可以理解为无记忆性。

    短连接
        在长连接出现之前，http1.0中都是使用的短连接，其特点是一次http交互完成后就会断开连接。
        由于http协议是基于TCP协议的，而TCP协议有三次握手四次挥手来建立以及断开连接，那么面对多次的http请求，
        这种短连接就会造成多次的握手以及挥手资源浪费，正是因为这个问题才出现了长连接。

    长连接
        在http1.1中，出现了http长连接，其特点是保持连接特性，当一次http交互完后该TCP通道并不会关闭，而是会保持一段时间(在不同服务器上时间不一样，可以设置)
        如果在这段时间内再次发起了http请求就可以直接复用，而不用重新进行握手，从而减少了资源浪费。目前http1.1中，都是默认使用长连接，在请求头中加上
        connection：keep-alive

    HTTP协议的长连接和短连接，实质上是TCP协议的长连接和短连接。

    长连接以及短连接的优缺点
        长连接：
            可以省去较多的TCP建立和关闭的操作，减少浪费，节约时间。
            对于频繁请求资源的客户来说，较适用长连接。不过这里存在一个问题，在长连接的应用场景下，client端一般不会主动关闭它们之间的连接，
            Client与server之间的连接如果一直不关闭的话，会存在一个问题，随着客户端连接越来越多，server早晚有扛不住的时候，这时候server端需要采取一些策略。
        短连接：
            对于服务器来说管理较为简单，存在的连接都是有用的连接，不需要额外的控制手段。但如果客户请求频繁，将在TCP的建立和关闭操作上浪费时间和带宽。
    ————————————————
    版权声明：本文为CSDN博主「三色之光」的原创文章，遵循CC 4.0 BY-SA版权协议，转载请附上原文出处链接及本声明。
    原文链接：https://blog.csdn.net/qq_41801117/article/details/118421304
    */
    //}
}

#endif

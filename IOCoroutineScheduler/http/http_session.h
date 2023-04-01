
#ifndef __SYLAR_HTTP_SESSION_H__
#define __SYLAR_HTTP_SESSION_H__

#include "IOCoroutineScheduler/streams/socket_stream.h"
#include "http.h"

namespace bin::http {
//namespace http {

    //HTTPSession封装
    /*将服务器端accept生成出来的socket封装成一个session结构，是一个被动发起的连接，负责收取来自客户端的HTTP请求报文，并且给客户端回发响应报文。
    执行两种任务：
        1、接收http请求
        2、发送http响应
    */
    class HttpSession : public SocketStream {
    public:
        typedef std::shared_ptr<HttpSession> ptr;

         HttpSession(Socket::ptr sock, bool owner = true);   //sock Socket类型 owner 是否托管

        HttpRequest::ptr recvRequest();             //核心函数：接收HTTP请求
        int sendResponse(HttpResponse::ptr rsp);    //核心函数：发送HTTP响应 rsp HTTP响应  返回：>0 成功 =0 对方关闭 <0 Socket异常
    };

//}
}

#endif
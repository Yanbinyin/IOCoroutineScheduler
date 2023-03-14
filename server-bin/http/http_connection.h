//HTTP客户端类

#ifndef __SYLAR_HTTP_CONNECTION_H__
#define __SYLAR_HTTP_CONNECTION_H__

#include "server-bin/streams/socket_stream.h"
#include "http.h"
#include "server-bin/uri.h"
#include "server-bin/thread.h"

#include <list>

namespace sylar::http {
//namespace http {

    //HTTP响应结果
    struct HttpResult {
        typedef std::shared_ptr<HttpResult> ptr;

        //错误码定义
        enum class Error {
            OK = 0,                         //正常 
            INVALID_URL = 1,                //非法URL
            INVALID_HOST = 2,               //无法解析HOST
            CONNECT_FAIL = 3,               //连接失败
            SEND_CLOSE_BY_PEER = 4,         //连接被对端关闭
            SEND_SOCKET_ERROR = 5,          //发送请求产生Socket错误
            TIMEOUT = 6,                    //超时
            CREATE_SOCKET_ERROR = 7,        //创建Socket失败
            POOL_GET_CONNECTION = 8,        //从连接池中取连接失败
            POOL_INVALID_CONNECTION = 9,    //无效的连接
        };

        //_result 错误码 _response HTTP响应结构体 _error 错误描述
        HttpResult(int _result, HttpResponse::ptr _response, const std::string& _error)
            :result(_result)
            ,response(_response)
            ,error(_error){}
        
        int result;                     //错误码
        HttpResponse::ptr response;     //HTTP响应结构体
        std::string error;              //错误描述
        std::string toString() const;
    };



    class HttpConnectionPool;

    /*HTTP客户端类
        将服务器创建出来发起主动连接的socket封装成一个Connection。
        和之前HttpSeesion类的功能一模一样，只是此时的服务器作为一个客户端对远端另外一台服务器发起资源请求。*/
    class HttpConnection : public SocketStream {
    friend class HttpConnectionPool;
    public:
        typedef std::shared_ptr<HttpConnection> ptr;

        //一般来说，GET没有body而POST有body，但是并没严格要求，GET可以有body，而POST也可以没有body
        //发送HTTP的GET请求 url 请求的url timeout_ms 超时时间(毫秒) headers HTTP请求头部参数 body 请求消息体
        static HttpResult::ptr DoGet(const std::string& url, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers = {}
                                , const std::string& body = "");
        //发送HTTP的GET请求,返回HTTP结果结构体 uri URI结构体 timeout_ms 超时时间(毫秒) headers HTTP请求头部参数 body 请求消息体
        static HttpResult::ptr DoGet(Uri::ptr uri, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers = {}
                                , const std::string& body = "");

        //发送HTTP的POST请求,返回HTTP结果结构体 url 请求的url timeout_ms 超时时间(ms) headers HTTP请求头部参数 body 请求消息体
        static HttpResult::ptr DoPost(const std::string& url, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers = {}
                                , const std::string& body = "");
        //发送HTTP的POST请求,返回HTTP结果结构体 uri URI结构体 timeout_ms 超时时间(ms) headers HTTP请求头部参数 body 请求消息体
        static HttpResult::ptr DoPost(Uri::ptr uri, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers = {}
                                , const std::string& body = "");


        /*核心函数：负责将HTTP请求发给目标服务器。有多个以此函数为基础，扩展出来的重载函数/便捷接口
          发送HTTP请求,返回HTTP结果结构体 req 请求结构体 uri URI结构体 timeout_ms 超时时间(毫秒)*/
        static HttpResult::ptr DoRequest(HttpRequest::ptr req, Uri::ptr uri, uint64_t timeout_ms);
        //发送HTTP请求,返回HTTP结果结构体 method 请求类型 uri 请求的url timeout_ms 超时时间(ms) headers HTTP请求头部参数 body 请求消息体
        static HttpResult::ptr DoRequest(HttpMethod method, const std::string& url, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers = {}
                                , const std::string& body = "");
        //核心函数：发送HTTP请求,返回HTTP结果结构体 method 请求类型 uri URI结构体 timeout_ms 超时时间(ms) headers HTTP请求头部参数 body 请求消息体
        static HttpResult::ptr DoRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers = {}
                                , const std::string& body = "");


        HttpConnection(Socket::ptr sock, bool owner = true);    //sock Socket类 owner 是否掌握所有权
        ~HttpConnection();

        HttpResponse::ptr recvResponse();       //核心函数：接收HTTP响应报文。支持HTTP chunck发包形式
        int sendRequest(HttpRequest::ptr req);  //发送HTTP请求 req HTTP请求结构

    private:
        uint64_t m_createTime = 0;  //连接创建时间
        uint64_t m_request = 0;     //连接上的请求数
    };



    /*利用"池化"技术将连接资源做一个统一管理，尤其对于"长连接"来讲，关注一个连接上的最大存活时间、最多能够处理的请求数量等，
    还要兼顾连接上的安全问题，规避一部分的攻击。并且，一个"池子"服务一个网址+一个端口，对于一个网址资源的请求连接数会很多。*/
    //power: 连接池同协议不同地址的，可以根据 分布式 进行 负载均衡 ，做出很多策略，比如轮询、带权重轮询、质量监控
    class HttpConnectionPool {
    public:
        typedef std::shared_ptr<HttpConnectionPool> ptr;
        typedef Mutex MutexType;

        static HttpConnectionPool::ptr Create(const std::string& uri, const std::string& vhost, uint32_t max_size
                        , uint32_t max_alive_time, uint32_t max_request);

        HttpConnectionPool(const std::string& host, const std::string& vhost, uint32_t port
                        , bool is_https, uint32_t max_size, uint32_t max_alive_time, uint32_t max_request);

        HttpConnection::ptr getConnection();

        //发送HTTP的GET请求,返回HTTP结果结构体 url 请求的url timeout_ms 超时时间(毫秒) headers HTTP请求头部参数 body 请求消息体
        HttpResult::ptr doGet(const std::string& url, uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");
        //发送HTTP的GET请求,返回HTTP结果结构体 uri URI结构体 timeout_ms 超时时间(毫秒) headers HTTP请求头部参数 body 请求消息体
        HttpResult::ptr doGet(Uri::ptr uri, uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");


        //发送HTTP的POST请求,返回HTTP结果结构体 url 请求的url timeout_ms 超时时间(毫秒) headers HTTP请求头部参数 body 请求消息体
        HttpResult::ptr doPost(const std::string& url, uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");
        //发送HTTP的POST请求,返回HTTP结果结构体 uri URI结构体 timeout_ms 超时时间(毫秒) headers HTTP请求头部参数 body 请求消息体
        HttpResult::ptr doPost(Uri::ptr uri, uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");


        //发送HTTP请求,返回HTTP结果结构体 method 请求类型 url 请求的url timeout_ms 超时时间(毫秒) headers HTTP请求头部参数 body 请求消息体
        HttpResult::ptr doRequest(HttpMethod method, const std::string& url, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers = {}
                                , const std::string& body = "");
        //发送HTTP请求,返回HTTP结果结构体 method 请求类型 uri URI结构体 timeout_ms 超时时间(毫秒) headers HTTP请求头部参数 body 请求消息体
        HttpResult::ptr doRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers = {}
                                , const std::string& body = "");
        //发送HTTP请求,返回HTTP结果结构体 req 请求结构体 timeout_ms 超时时间(毫秒)
        HttpResult::ptr doRequest(HttpRequest::ptr req, uint64_t timeout_ms);

    private:
        static void ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool);

    private:
        std::string m_host;     //要连接的主机域名
        std::string m_vhost;
        uint32_t m_port;        //端口号
        uint32_t m_maxSize;     //最大连接数  并不表示连接池最大有这么多连接，如果连接池拿完了新的请求过来了，还是要创建，满了之后的连接不用了直接释放掉
        uint32_t m_maxAliveTime;//每条连接最大存活时间
        uint32_t m_maxRequest;  //每条连接上最大请求数量
        bool m_isHttps;

        MutexType m_mutex;                  //互斥锁 因为读写频次相当，不用读写锁
        std::list<HttpConnection*> m_conns; //连接存储容器  list增删方便
        std::atomic<int32_t> m_total = {0}; //当前连接数量 可以大于最大连接数 空间不够了新创建空间存储 但是使用完毕放回后若超限就要马上释放
    };

//}
}

#endif

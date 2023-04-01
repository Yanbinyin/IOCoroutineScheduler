#include "http_connection.h"
#include "http_parser.h"
#include "IOCoroutineScheduler/log.h"
// #include "IOCoroutineScheduler/streams/zlib_stream.h"

namespace bin::http{
//namespace http {

    static bin::Logger::ptr g_logger = BIN_LOG_NAME("system");




    std::string HttpResult::toString() const {
        std::stringstream ss;
        ss << "[HttpResult result=" << result
        << " error=" << error
        << " response=" << (response ? response->toString() : "nullptr")
        << "]";
        return ss.str();
    }




    HttpConnection::HttpConnection(Socket::ptr sock, bool owner)
        :SocketStream(sock, owner){
    }

    HttpConnection::~HttpConnection(){
        BIN_LOG_DEBUG(g_logger) << "HttpConnection::~HttpConnection";
    }

    HttpResponse::ptr HttpConnection::recvResponse(){
        //power: 先解析出 报文头部，再解析 报文实体

        //1. 创建HttpRequestParser对象，获取当前所能接收的最大报文首部长度，根据这个长度创建一块缓冲区用于暂存接收到的请求报文
        HttpResponseParser::ptr parser(new HttpResponseParser);
        uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize(); //使用某一时刻的值即可 不需要实时的值
        //uint64_t buff_size = 100;

        //创建一个接受响应报文的缓冲区 指定析构
        std::shared_ptr<char> buffer(
                new char[buff_size + 1], [](char* ptr){
                    delete[] ptr;
        });
        char* data = buffer.get();
        int offset = 0;

        /*2. 采取一边接收报文，一边解析报文头部的策略：
        while(1){
            a. 尝试通过套接字Socket对象接收报文
            b. 根据实际接收到的报文长度len，尝试调用HttpResponseParser::execute()执行报文解析动作，并且计算一个偏移量 offset = 实际接收到的报文长度 - 实际解析的报文长度， 判断当前的报文是否超过最大报文首部限定值（4KB）。
            c. 判断解析是否已经全部完成：是，就执行break；否则，继续接收报文，继续解析。
        }*/
        do {
            int len = read(data + offset, buff_size - offset);
            if(len <= 0){
                close();
                return nullptr;
            }
            len += offset;
            data[len] = '\0'; //httpclient_parser.rl文件中存在assert(*pe == '\0')的断言
            size_t nparse = parser->execute(data, len, false); //开启分块解析，返回实际解析的长度
            if(parser->hasError()){
                close();
                return nullptr;
            }

            offset = len - nparse;
            if(offset == (int)buff_size){ //读了数据但是没有解析  直到自己定义的缓存已经满了
                BIN_LOG_WARN(g_logger) << "HttpConnection::recvResponse http response buffer out of  range";
                close();
                return nullptr;
            }
            if(parser->isFinished()) //如果解析已经结束
                break;
        } while(true);

        /*3. 根据解析出的报文首部，看首部字段Transfer-Encoding:chunked，对应到代码中是之前HTTP解析模块中httpclient_parser:chuncked是否被置
             值为1：是，就表明接下来的响应报文实体将以chunck形式发送；否则，就是首部报文后紧跟需要的报文实体。*/
        auto& client_parser = parser->getParser();
        std::string body;
        
        //3.1  报文实体是chunck形式： chunked形式分块发送，就需要分块接收
        if(client_parser.chunked){
            int len = offset; //拿到剩余报文的长度
            do{
                bool begin = true;
                do{
                    //ⅰ. 尝试读取一次 剩余的缓存空间大小的数据 尽可能的多读出主体
                    if(!begin || len == 0){
                        int rt = read(data + len, buff_size - len);
                        if(rt <= 0){
                            close();
                            return nullptr;
                        }
                        len += rt;
                    }
                    data[len] = '\0';  //httpclient_parser.rl文件中存在assert(*pe == '\0')的断言
                    
                    //ⅱ. 调用HttpResponseParser::execute()进行解析。
                    //    同样需要计算一个偏移量len = 实际接收到的报文长度 - 实际解析的报文长度判断当前的报文是否超过最大报文首部限定值（4KB）
                    size_t nparse = parser->execute(data, len, true);
                    if(parser->hasError()){
                        close();
                        return nullptr;
                    }
                    len -= nparse;
                    if(len == (int)buff_size){ //一个都没解析 读了数据但是没有解析，直到自己定义的缓存已经满了
                        close();
                        return nullptr;
                    }
                    begin = false;
                }while(!parser->isFinished());//当前报文分块是否解析完成

                //减2非常关键 因为报文首部和实体中间还有一个空行\r\n将它算作首部的长度 不应算在body的长度中
                //len -= 2; //这里bin后面注释掉了，相应的if判断涉及len的都+2
                
                BIN_LOG_DEBUG(g_logger) << "content_len=" << client_parser.content_len;

                /*如果当前解析包中的主体长度 <= 当前已经接受到的并且解析好的报文主体长度
                  说明缓冲区中当前分块的数据量较少，另一个分块的的数据量较多。将client_parser.content_len这么多的数据放入body，
                  然后使用memmove()将缓冲区中的未填装好的数据挪到缓冲区前覆盖已经填装掉掉的数据区域*/
                if(client_parser.content_len + 2 <= len){
                    body.append(data, client_parser.content_len);
                    memmove(data, data + client_parser.content_len + 2
                            , len - client_parser.content_len - 2);
                    len -= client_parser.content_len + 2;
                }
                /*如果当前解析包中的主体长度 > 当前已经接受到的并且解析好的报文主体长度
                  说明当前缓冲区中所有的数据都是当前分块的，并且还有一部分数据没有接受到，还需要等待接收。
                  将len（当前缓冲区中所有的数据）长度的数据放入body，计算剩余待接收的数据量left，通过read直接读取缓冲区中，又放入body中。*/
                else{                    
                    body.append(data, len);
                    int left = client_parser.content_len - len + 2;

                    //最后将完整的报文实体放入Body，后面放入HttpRequestParser::HttpRequest对象之中
                    while(left > 0){
                        int rt = read(data, left > (int)buff_size ? (int)buff_size : left);
                        if(rt <= 0){
                            close();
                            return nullptr;
                        }
                        body.append(data, rt);
                        left -= rt;
                    }

                    body.resize(body.size() - 2);
                    len = 0; //读完 len设置为0 进入下一个窗口去读
                }
            } while(!client_parser.chunks_done); //所有分块是否全部接受完毕
        }
        /*3.2. 报文实体非chunck形式：（做法就和recvRequest()中一样）*/
        else{
            //存储报文实体内容。获取报文实体的长度大小，根据报文实体长度 和 2. 中得到的一个偏移量offset来判断一下当前的报文实体是否接收完整：
            int64_t length = parser->getContentLength(); //获取实体长度
            if(length > 0){
                body.resize(length);
                int len = 0;
                if(length >= offset){
                    memcpy(&body[0], data, offset);
                    len = offset;
                }else{
                    memcpy(&body[0], data, length);
                    len = length;
                }
                //a. 报文实体长度 > offset， 说明当前收到的报文实体不完整。通过readFixSize函数的循环，再一次接收客户端传输来的报文实体，直至报文实体完整。
                length -= offset;
                if(length > 0){
                    if(readFixSize(&body[len], length) <= 0){
                        close();
                        return nullptr;
                    }
                }
                //b. 报文实体长度 <=  offset，说明当前收到的报文实体已经完整
            }
        }
        //3.3 将完整的报文实体放入HttpRequestParser::HttpRequest对象之中
        if(!body.empty()){
            auto content_encoding = parser->getData()->getHeader("content-encoding");
            BIN_LOG_DEBUG(g_logger) << "content_encoding: " << content_encoding << " size=" << body.size();
            // //hack:看不懂这一堆zs的目的 //todo: 为了编译注释掉这里和头文件
            // if(strcasecmp(content_encoding.c_str(), "gzip") == 0){
            //     auto zs = ZlibStream::CreateGzip(false);
            //     zs->write(body.c_str(), body.size());
            //     zs->flush();
            //     zs->getResult().swap(body);
            // } else if(strcasecmp(content_encoding.c_str(), "deflate") == 0){
            //     auto zs = ZlibStream::CreateDeflate(false);
            //     zs->write(body.c_str(), body.size());
            //     zs->flush();
            //     zs->getResult().swap(body);
            // }
            parser->getData()->setBody(body); //将完整的报文实体放入HttpRequestParser::HttpRequest对象之中
        }
        return parser->getData();
    }

    int HttpConnection::sendRequest(HttpRequest::ptr rsp){
        std::stringstream ss;
        ss << *rsp;
        std::string data = ss.str();
        //std::cout << ss.str() << std::endl;
        return writeFixSize(data.c_str(), data.size());
    }

    HttpResult::ptr HttpConnection::DoGet(const std::string& url, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body){
        Uri::ptr uri = Uri::Create(url);
        if(!uri){
            return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL
                    , nullptr, "invalid url: " + url);
        }
        return DoGet(uri, timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnection::DoGet(Uri::ptr uri, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body){
        return DoRequest(HttpMethod::GET, uri, timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnection::DoPost(const std::string& url
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body){
        Uri::ptr uri = Uri::Create(url);
        if(!uri){
            return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL
                    , nullptr, "invalid url: " + url);
        }
        return DoPost(uri, timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnection::DoPost(Uri::ptr uri
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body){
        return DoRequest(HttpMethod::POST, uri, timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnection::DoRequest(HttpMethod method
                                , const std::string& url
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body){
        Uri::ptr uri = Uri::Create(url);
        if(!uri){
            return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL
                    , nullptr, "invalid url: " + url);
        }
        return DoRequest(method, uri, timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body){
        HttpRequest::ptr req = std::make_shared<HttpRequest>();
        req->setPath(uri->getPath());           //设置请求资源路径
        req->setQuery(uri->getQuery());         //设置查询参数
        req->setFragment(uri->getFragment());   //设置分段标识符
        req->setMethod(method);                 //设置请求方法
        bool has_host = false;
        for(auto& i : headers){
            if(strcasecmp(i.first.c_str(), "connection") == 0){
                if(strcasecmp(i.second.c_str(), "keep-alive") == 0){
                    req->setClose(false); //设置为长连接
                }
                continue;
            }

            if(!has_host && strcasecmp(i.first.c_str(), "host") == 0){
                has_host = !i.second.empty();
            }

            req->setHeader(i.first, i.second);
        }
        if(!has_host){
            req->setHeader("Host", uri->getHost());
        }
        req->setBody(body);
        return DoRequest(req, uri, timeout_ms);
    }

    HttpResult::ptr HttpConnection::DoRequest(HttpRequest::ptr req, Uri::ptr uri, uint64_t timeout_ms){
        bool is_ssl = uri->getScheme() == "https";

        //通过URI对象智能指针创建地址对象
        Address::ptr addr = uri->createAddress();
        if(!addr){
            return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_HOST
                    , nullptr, "invalid host: " + uri->getHost());
        }

        //通过地址对象创建套接字对象
        Socket::ptr sock = is_ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
        if(!sock){
            return std::make_shared<HttpResult>((int)HttpResult::Error::CREATE_SOCKET_ERROR
                    , nullptr, "create socket fail: " + addr->toString()
                            + " errno=" + std::to_string(errno)
                            + " errstr=" + std::string(strerror(errno)));
        }

        //通过套接字连接目标服务器
        if(!sock->connect(addr)){
            return std::make_shared<HttpResult>((int)HttpResult::Error::CONNECT_FAIL
                    , nullptr, "connect fail: " + addr->toString());
        }
        sock->setRecvTimeout(timeout_ms); //为套接字设置接收超时时间
        HttpConnection::ptr conn = std::make_shared<HttpConnection>(sock); //通过连接成功的套接字创建HTTP连接
        int rt = conn->sendRequest(req); //通过连接发送HTTP请求报文
        if(rt == 0){
            return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSE_BY_PEER
                    , nullptr, "send request closed by peer: " + addr->toString());
        }
        if(rt < 0){
            return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_SOCKET_ERROR
                        , nullptr, "send request socket error errno=" + std::to_string(errno)
                        + " errstr=" + std::string(strerror(errno)));
        }
        //通过连接接收HTTP请求报文
        auto rsp = conn->recvResponse();
        if(!rsp){
            return std::make_shared<HttpResult>((int)HttpResult::Error::TIMEOUT
                        , nullptr, "recv response timeout: " + addr->toString()
                        + " timeout_ms:" + std::to_string(timeout_ms));
        }
        //返回HTTP处理结果的结构体指针指针
        return std::make_shared<HttpResult>((int)HttpResult::Error::OK, rsp, "ok");
    }

    HttpConnectionPool::ptr HttpConnectionPool::Create(const std::string& uri
                                                    ,const std::string& vhost
                                                    ,uint32_t max_size
                                                    ,uint32_t max_alive_time
                                                    ,uint32_t max_request){
        Uri::ptr turi = Uri::Create(uri);
        if(!turi){
            BIN_LOG_ERROR(g_logger) << "invalid uri=" << uri;
        }
        return std::make_shared<HttpConnectionPool>(turi->getHost()
                , vhost, turi->getPort(), turi->getScheme() == "https"
                , max_size, max_alive_time, max_request);
    }

    HttpConnectionPool::HttpConnectionPool(const std::string& host
                                            ,const std::string& vhost
                                            ,uint32_t port
                                            ,bool is_https
                                            ,uint32_t max_size
                                            ,uint32_t max_alive_time
                                            ,uint32_t max_request)
        :m_host(host)
        ,m_vhost(vhost)
        ,m_port(port ? port : (is_https ? 443 : 80))
        ,m_maxSize(max_size)
        ,m_maxAliveTime(max_alive_time)
        ,m_maxRequest(max_request)
        ,m_isHttps(is_https){
    }

    HttpConnection::ptr HttpConnectionPool::getConnection(){
        uint64_t now_ms = bin::GetCurrentMS();
        std::vector<HttpConnection*> invalid_conns;
        HttpConnection* ptr = nullptr;
        MutexType::Lock lock(m_mutex);
        while(!m_conns.empty()){
            auto conn = *m_conns.begin();
            m_conns.pop_front();
            if(!conn->isConnected()){
                invalid_conns.push_back(conn);
                continue;
            }
            if((conn->m_createTime + m_maxAliveTime) > now_ms){
                invalid_conns.push_back(conn);
                continue;
            }
            ptr = conn;
            break;
        }
        lock.unlock();
        for(auto i : invalid_conns){
            delete i;
        }
        m_total -= invalid_conns.size();

        if(!ptr){ //没有值需要去创建
            IPAddress::ptr addr = Address::LookupAnyIPAddress(m_host);
            if(!addr){
                BIN_LOG_ERROR(g_logger) << "get addr fail: " << m_host;
                return nullptr;
            }
            addr->setPort(m_port);
            Socket::ptr sock = m_isHttps ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
            if(!sock){
                BIN_LOG_ERROR(g_logger) << "create sock fail: " << *addr;
                return nullptr;
            }
            if(!sock->connect(addr)){
                BIN_LOG_ERROR(g_logger) << "sock connect fail: " << *addr;
                return nullptr;
            }

            ptr = new HttpConnection(sock);
            ++m_total;
        }
        return HttpConnection::ptr(ptr, std::bind(&HttpConnectionPool::ReleasePtr
                                , std::placeholders::_1, this));
    }

    void HttpConnectionPool::ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool){
        ++ptr->m_request;
        if(!ptr->isConnected()
                || ((ptr->m_createTime + pool->m_maxAliveTime) >= bin::GetCurrentMS())
                || (ptr->m_request >= pool->m_maxRequest)){
            delete ptr;
            --pool->m_total;
            return;
        }
        MutexType::Lock lock(pool->m_mutex);
        pool->m_conns.push_back(ptr);
    }

    HttpResult::ptr HttpConnectionPool::doGet(const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers
                            , const std::string& body){
        return doRequest(HttpMethod::GET, url, timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnectionPool::doGet(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers
                                    , const std::string& body){
        std::stringstream ss;
        ss << uri->getPath()
        << (uri->getQuery().empty() ? "" : "?")
        << uri->getQuery()
        << (uri->getFragment().empty() ? "" : "#")
        << uri->getFragment();
        return doGet(ss.str(), timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnectionPool::doPost(const std::string& url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers
                                    , const std::string& body){
        return doRequest(HttpMethod::POST, url, timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnectionPool::doPost(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers
                                    , const std::string& body){
        std::stringstream ss;
        ss << uri->getPath()
        << (uri->getQuery().empty() ? "" : "?")
        << uri->getQuery()
        << (uri->getFragment().empty() ? "" : "#")
        << uri->getFragment();
        return doPost(ss.str(), timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method
                                        , const std::string& url
                                        , uint64_t timeout_ms
                                        , const std::map<std::string, std::string>& headers
                                        , const std::string& body){
        HttpRequest::ptr req = std::make_shared<HttpRequest>();
        req->setPath(url);
        req->setMethod(method);
        req->setClose(false); //设置为长连接
        bool has_host = false;
        for(auto& i : headers){
            if(strcasecmp(i.first.c_str(), "connection") == 0){
                if(strcasecmp(i.second.c_str(), "keep-alive") == 0){
                    req->setClose(false); //设置为长连接
                }
                continue;
            }

            if(!has_host && strcasecmp(i.first.c_str(), "host") == 0){
                has_host = !i.second.empty();
            }

            req->setHeader(i.first, i.second);
        }
        if(!has_host){
            if(m_vhost.empty()){
                req->setHeader("Host", m_host);
            }else{
                req->setHeader("Host", m_vhost);
            }
        }
        req->setBody(body);
        return doRequest(req, timeout_ms);
    }

    HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method
                                        , Uri::ptr uri
                                        , uint64_t timeout_ms
                                        , const std::map<std::string, std::string>& headers
                                        , const std::string& body){
        std::stringstream ss;
        ss << uri->getPath()
        << (uri->getQuery().empty() ? "" : "?")
        << uri->getQuery()
        << (uri->getFragment().empty() ? "" : "#")
        << uri->getFragment();
        return doRequest(method, ss.str(), timeout_ms, headers, body);
    }

    HttpResult::ptr HttpConnectionPool::doRequest(HttpRequest::ptr req
                                            , uint64_t timeout_ms){
        auto conn = getConnection();
        if(!conn){
            return std::make_shared<HttpResult>((int)HttpResult::Error::POOL_GET_CONNECTION
                    , nullptr, "pool host:" + m_host + " port:" + std::to_string(m_port));
        }
        auto sock = conn->getSocket();
        if(!sock){
            return std::make_shared<HttpResult>((int)HttpResult::Error::POOL_INVALID_CONNECTION
                    , nullptr, "pool host:" + m_host + " port:" + std::to_string(m_port));
        }
        sock->setRecvTimeout(timeout_ms);
        int rt = conn->sendRequest(req);
        if(rt == 0){
            return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSE_BY_PEER
                    , nullptr, "send request closed by peer: " + sock->getRemoteAddress()->toString());
        }
        if(rt < 0){
            return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_SOCKET_ERROR
                        , nullptr, "send request socket error errno=" + std::to_string(errno)
                        + " errstr=" + std::string(strerror(errno)));
        }
        auto rsp = conn->recvResponse();
        if(!rsp){
            return std::make_shared<HttpResult>((int)HttpResult::Error::TIMEOUT
                        , nullptr, "recv response timeout: " + sock->getRemoteAddress()->toString()
                        + " timeout_ms:" + std::to_string(timeout_ms));
        }
        return std::make_shared<HttpResult>((int)HttpResult::Error::OK, rsp, "ok");
    }

//}
}

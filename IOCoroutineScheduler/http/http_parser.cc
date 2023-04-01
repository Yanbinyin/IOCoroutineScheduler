#include "http_parser.h"
#include "IOCoroutineScheduler/log.h"
#include "IOCoroutineScheduler/config.h"
#include <string.h>

namespace bin::http {
//namespace http {

    static bin::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    //block: 配置项
    //配置项 规定一个请求报文首部字段数据长度阈值默认4KB  来规避大数据发包攻击
    static bin::ConfigVar<uint64_t>::ptr g_http_request_buffer_size =
            bin::Config::Lookup("http.request.buffer_size", (uint64_t)(4 * 1024), "http request buffer size");

    //配置项 规定一个请求报文报文实体数据长度阈值默认64MB
    static bin::ConfigVar<uint64_t>::ptr g_http_request_max_body_size =
            bin::Config::Lookup("http.request.max_body_size", (uint64_t)(64 * 1024 * 1024), "http request max body size");

    //配置项 规定一个响应报文首部字段数据长度阈值默认4KB  来规避大数据发包攻击
    static bin::ConfigVar<uint64_t>::ptr g_http_response_buffer_size =
            bin::Config::Lookup("http.response.buffer_size", (uint64_t)(4 * 1024), "http response buffer size");

    //配置项 规定一个响应报文报文实体数据长度阈值默认64MB
    static bin::ConfigVar<uint64_t>::ptr g_http_response_max_body_size =
            bin::Config::Lookup("http.response.max_body_size", (uint64_t)(64 * 1024 * 1024), "http response max body size");

    static uint64_t s_http_request_buffer_size = 0;     //当前请求报文首部字段长度
    static uint64_t s_http_request_max_body_size = 0;   //当前请求报文实体数据长度
    static uint64_t s_http_response_buffer_size = 0;    //当前响应报文首部字段长度
    static uint64_t s_http_response_max_body_size = 0;  //当前响应报文实体数据长度

    uint64_t HttpRequestParser::GetHttpRequestBufferSize(){
        return s_http_request_buffer_size;
    }

    uint64_t HttpRequestParser::GetHttpRequestMaxBodySize(){
        return s_http_request_max_body_size;
    }

    uint64_t HttpResponseParser::GetHttpResponseBufferSize(){
        return s_http_response_buffer_size;
    }

    uint64_t HttpResponseParser::GetHttpResponseMaxBodySize(){
        return s_http_response_max_body_size;
    }

    //于main函数之前初始化配置项，并给这些配置项设置回调，一旦发生修改就自动适应新值
    namespace { //初始化结构放在匿名空间 防止污染
        struct _RequestSizeIniter {
            _RequestSizeIniter(){
                s_http_request_buffer_size = g_http_request_buffer_size->getValue();
                s_http_request_max_body_size = g_http_request_max_body_size->getValue();
                s_http_response_buffer_size = g_http_response_buffer_size->getValue();
                s_http_response_max_body_size = g_http_response_max_body_size->getValue();
                
                g_http_request_buffer_size->addListener(
                            [](const uint64_t& ov, const uint64_t& nv){ s_http_request_buffer_size = nv; }
                        );

                g_http_request_max_body_size->addListener(
                            [](const uint64_t& ov, const uint64_t& nv){ s_http_request_max_body_size = nv; }
                        );

                g_http_response_buffer_size->addListener(
                            [](const uint64_t& ov, const uint64_t& nv){ s_http_response_buffer_size = nv; }
                        );

                g_http_response_max_body_size->addListener(
                            [](const uint64_t& ov, const uint64_t& nv){ s_http_response_max_body_size = nv; }
                        );
            }
        };

        static _RequestSizeIniter _init;
    }



    //block: 初始化 HttpRequestParser 成员变量m_parser需要的函数
    /*//power: 根据HTTP请求报文不同部分，调用相应的回调函数处理。 仿照开源项目给出的函数签名形式，定义我们自己的解析回调函数
    ● http11_common.h:
    该头文件中给出了回调函数所需的函数签名，有两种类型
        typedef void (*element_cb)(void *data, const char *at, size_t length);
        typedef void (*field_cb)(void *data, const char *field, size_t flen, const char *value, size_t vlen);
    */

    //解析HTTP请求方法回调函数
    void on_request_method(void *data, const char *at, size_t length){
        HttpRequestParser* parser = static_cast<HttpRequestParser*>(data); //拿到this指针
        HttpMethod m = CharsToHttpMethod(at);

        if(m == HttpMethod::INVALID_METHOD){
            SYLAR_LOG_WARN(g_logger) << "invalid http request method: " << std::string(at, length);
            parser->setError(1000);
            return;
        }
        parser->getData()->setMethod(m);
    }

    //解析URI回调函数 要自定义URI的解析故不使用
    void on_request_uri(void *data, const char *at, size_t length){
    }

    //解析分段标识符回调函数
    void on_request_fragment(void *data, const char *at, size_t length){
        //SYLAR_LOG_INFO(g_logger) << "on_request_fragment:" << std::string(at, length);
        HttpRequestParser* parser = static_cast<HttpRequestParser*>(data); //拿到this指针
        parser->getData()->setFragment(std::string(at, length));
    }

    //解析资源路径回调函数
    void on_request_path(void *data, const char *at, size_t length){
        HttpRequestParser* parser = static_cast<HttpRequestParser*>(data); //拿到this指针
        parser->getData()->setPath(std::string(at, length));
    }

    //解析查询参数回调函数
    void on_request_query(void *data, const char *at, size_t length){
        HttpRequestParser* parser = static_cast<HttpRequestParser*>(data); //拿到this指针
        parser->getData()->setQuery(std::string(at, length));
    }

    //解析HTTP协议版本回调函数
    void on_request_version(void *data, const char *at, size_t length){
        HttpRequestParser* parser = static_cast<HttpRequestParser*>(data); //拿到this指针
        uint8_t v = 0;
        if(strncmp(at, "HTTP/1.1", length) == 0){
            v = 0x11;
        } else if(strncmp(at, "HTTP/1.0", length) == 0){
            v = 0x10;
        }else{
            SYLAR_LOG_WARN(g_logger) << "invalid http request version: "
                << std::string(at, length);
            parser->setError(1001);
            return;
        }
        parser->getData()->setVersion(v);
    }

    void on_request_header_done(void *data, const char *at, size_t length){
        //HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    }

    //解析一系列首部字段的回调函数
    void on_request_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen){
        HttpRequestParser* parser = static_cast<HttpRequestParser*>(data); //拿到this指针
        if(flen == 0){
            SYLAR_LOG_WARN(g_logger) << "invalid http request field length == 0";
            //parser->setError(1002); //invalid field
            return;
        }
        parser->getData()->setHeader(std::string(field, flen), std::string(value, vlen));
    }



    //block: HttpRequestParser
    //对解析请求报文的结构体httpclient_parser进行初始化，需要对报文每一部分的解析指定对应的一个回调函数
    HttpRequestParser::HttpRequestParser()
            :m_error(0){
        m_data.reset(new bin::http::HttpRequest);
        //初始化http_parser m_parser的所有成员
        http_parser_init(&m_parser);
        m_parser.request_method = on_request_method;
        m_parser.request_uri = on_request_uri;
        m_parser.fragment = on_request_fragment;
        m_parser.request_path = on_request_path;
        m_parser.query_string = on_request_query;
        m_parser.http_version = on_request_version;
        m_parser.header_done = on_request_header_done;
        m_parser.http_field = on_request_http_field;
        m_parser.data = this;
    }

    uint64_t HttpRequestParser::getContentLength(){
        return m_data->getHeaderAs<uint64_t>("content-length", 0);
    }

    /*核心函数：执行解析动作，进行一次对HTTP请求报文的解析。
        这个借口比较特殊，使用了开源项目中的http_parser_execute()，该函数是一种状态机的调用形式：
        解析不同报文部分，会自动调用不同的回调函数去进行自动的解析。
    //1: 成功
    //-1: 有错误
    //>0: 已处理的字节数，且data有效数据为len - v;    
    */
    size_t HttpRequestParser::execute(char* data, size_t len){
        size_t offset = http_parser_execute(&m_parser, data, len, 0);   //power: 解析，这里是关键
        //先将解析过的空间挪走 防止缓存不够 但是仍然有数据位解析完成的情况
        memmove(data, data + offset, (len - offset));
        //返回实际解析过的字节数
        return offset;
    }

    int HttpRequestParser::isFinished(){
        return http_parser_finish(&m_parser);
    }

    int HttpRequestParser::hasError(){
        return m_error || http_parser_has_error(&m_parser);
    }




    //block: 初始化 httpclient_parser 成员变量m_parser需要的函数

    /*根据HTTP响应报文不同部分，调用相应的回调函数处理。 仿照开源项目给出的函数签名形式，定义我们自己的解析回调函数*/
    //解析状态原因短语回调函数
    void on_response_reason(void *data, const char *at, size_t length){
        HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
        parser->getData()->setReason(std::string(at, length));
    }

    //解析状态码回调函数
    void on_response_status(void *data, const char *at, size_t length){
        HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
        HttpStatus status = (HttpStatus)(atoi(at));
        parser->getData()->setStatus(status);
    }

    //解析HTTP协议版本回调函数
    void on_response_chunk(void *data, const char *at, size_t length){
    }

    void on_response_version(void *data, const char *at, size_t length){
        HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
        uint8_t v = 0;
        if(strncmp(at, "HTTP/1.1", length) == 0){
            v = 0x11;
        } else if(strncmp(at, "HTTP/1.0", length) == 0){
            v = 0x10;
        }else{
            SYLAR_LOG_WARN(g_logger) << "invalid http response version: "
                << std::string(at, length);
            parser->setError(1001);
            return;
        }

        parser->getData()->setVersion(v);
    }

    void on_response_header_done(void *data, const char *at, size_t length){
    }

    void on_response_last_chunk(void *data, const char *at, size_t length){
    }

    //解析一系列首部字段回调函数
    void on_response_http_field(void *data, const char *field, size_t flen
                            ,const char *value, size_t vlen){
        HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
        if(flen == 0){
            SYLAR_LOG_WARN(g_logger) << "invalid http response field length == 0";
            //parser->setError(1002); //invalid field
            return;
        }
        parser->getData()->setHeader(std::string(field, flen), std::string(value, vlen));
    }



    //block: HttpResponseParser

    //对解析请求报文的结构体httpclient_parser进行初始化，需要对报文每一部分的解析指定对应的一个回调函数
    HttpResponseParser::HttpResponseParser()
        :m_error(0){
        m_data.reset(new bin::http::HttpResponse);
        httpclient_parser_init(&m_parser);
        m_parser.reason_phrase = on_response_reason;
        m_parser.status_code = on_response_status;
        m_parser.chunk_size = on_response_chunk;
        m_parser.http_version = on_response_version;
        m_parser.header_done = on_response_header_done;
        m_parser.last_chunk = on_response_last_chunk;
        m_parser.http_field = on_response_http_field;
        m_parser.data = this; //this指针放入
    }

    /*执行解析动作，进行一次对HTTP响应报文的解析。这个借口比较特殊，使用了开源项目中的httpclient_parser_execute()，该函数是一种状态机的调用形式：
        解析不同报文部分，会自动调用不同的回调函数去进行自动的解析，然后判断这一段是解析成功还是解析失败。
    和HTTP请求报文解析不同的点在于，响应报文不一定一次就全部发送完毕。可能存在分块发送的情况，即：chunck形式。
        但是复用的开源项目里并不支持对报文的分段解析，需要我们特殊处理：
        如果为chunck发包形式的话，每一次解析就要重置解析结构体的状态，防止记录上一个包的状态。 */
    size_t HttpResponseParser::execute(char* data, size_t len, bool chunck){
        if(chunck){ //如果为chunck包需要重新初始化一下解析包
            httpclient_parser_init(&m_parser);
        }
        //每一次都是从头开始解析
        size_t offset = httpclient_parser_execute(&m_parser, data, len, 0); //power: 这里是关键
        //先将解析过的空间挪走
        memmove(data, data + offset, (len - offset));
        //返回实际解析过的字节数
        return offset;
    }

    int HttpResponseParser::isFinished(){
        return httpclient_parser_finish(&m_parser);
    }

    int HttpResponseParser::hasError(){
        return m_error || httpclient_parser_has_error(&m_parser);
    }

    uint64_t HttpResponseParser::getContentLength(){
        return m_data->getHeaderAs<uint64_t>("content-length", 0);
    }

//}
}

//HTTP协议解析封装

#ifndef __SYLAR_HTTP_PARSER_H__
#define __SYLAR_HTTP_PARSER_H__

#include "http.h"
#include "http11_parser.h"
#include "httpclient_parser.h"

namespace sylar::http {
//namespace http {

    //Http请求解析结构体
    /*真正在使用封装好的HTTP解析的接口的时候，解析数据不会一次性将HTTP报文数据接收完整，
        由于网络延迟、数据量大等原因，发送的时候会拆分为多个报文小段进行发送，解析的时候也是一小段一小段进行解析，
        解析完一次就要返回一个状态去判别下一步应该进行什么操作。*/
    class HttpRequestParser {
    public:
        typedef std::shared_ptr<HttpRequestParser> ptr;

        HttpRequestParser();

        //核心函数：解析协议，返回实际解析的长度,并且将已解析的数据移除 data 协议文本内存  len 协议文本内存长度
        size_t execute(char* data, size_t len);

        int isFinished();                                           //判断当前这次报文解析是否结束。复用开源项目的http_parser_finish()函数
        int hasError();                                             //判断当前这次报文解析是否出错。复用开源项目的http_parser_has_error()函数。
        void setError(int v){ m_error = v;}                        //设置解析时的错误码(v) 
        uint64_t getContentLength();                                //获取报文主体字段"content-length"中的长度，并将该值由string转换为uint64_t
        HttpRequest::ptr getData() const { return m_data;}          //返回HttpRequest结构体
        const http_parser& getParser() const { return m_parser;}    //获取http_parser结构体

    public:
        static uint64_t GetHttpRequestBufferSize();     //返回HttpRequest协议解析的请求报文头部最大缓存区大小
        static uint64_t GetHttpRequestMaxBodySize();    //返回HttpRequest协议的最大消息体大小

    private:
        http_parser m_parser;       //http_parser，解析请求报文的结构体
        HttpRequest::ptr m_data;    //HttpRequest结构，请求报文对象智能指针

        //错误码
        //1000: invalid method
        //1001: invalid version
        //1002: invalid field
        int m_error;
    };



    //Http响应解析结构体
    class HttpResponseParser {
    public:
        typedef std::shared_ptr<HttpResponseParser> ptr;

        HttpResponseParser();

        //解析HTTP响应协议，返回实际解析的长度,并且移除已解析的数据  data 协议数据内存  len 协议数据内存大小  chunck 是否在解析chunck
        size_t execute(char* data, size_t len, bool chunck);

        int isFinished();                                               //判断当前这次报文解析是否结束。复用开源项目的httpclient_parser_finish()函数
        int hasError();                                                 //判断当前这次报文解析是否出错。复用开源项目的httpclient_parser_has_error()函数
        void setError(int v){ m_error = v;}                            //设置解析时的错误码
        uint64_t getContentLength();                                    //获取报文主体字段"content-length"中的长度，并将该值由string转换为uint64_t
        HttpResponse::ptr getData() const { return m_data;}             //获取HTTP响应报文，返回HttpResponse
        const httpclient_parser& getParser() const { return m_parser;}  //返回httpclient_parser

    public:
        static uint64_t GetHttpResponseBufferSize();    //获取请求报文头部最大缓冲区大小，返回HTTP响应解析缓存大小
        static uint64_t GetHttpResponseMaxBodySize();   //获取请求报文主体最大大小，返回HTTP响应最大消息体大小

    private:
        httpclient_parser m_parser; //解析响应报文的结构体
        HttpResponse::ptr m_data;   //响应报文对象

        //错误码
        //1001: invalid version
        //1002: invalid field
        int m_error;
    };

//}
}

#endif

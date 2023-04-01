#include "IOCoroutineScheduler/http/http_parser.h"
#include "IOCoroutineScheduler/log.h"

static bin::Logger::ptr g_logger = SYLAR_LOG_ROOT();

//power: string还有字符数组，可以用“” 无限拼接，string str = "str" "ing" "s"; //等价于string str = "strings";
//这里是一个const char test_requeset_data[71];
const char test_request_data[] = "POST / HTTP/1.1\r\n"
                                "Host: www.bin.top\r\n"
                                "Content-Length: 10\r\n\r\n" //power: head和body之间有一个空行，打印出来空两行是因为日志结束也默认空一行
                                "1234567890";

void test_request(){
    bin::http::HttpRequestParser parser;
    std::string tmp = test_request_data;
    size_t s = parser.execute(&tmp[0], tmp.size()); //s: 解析的长度
    SYLAR_LOG_ERROR(g_logger) << "execute rt=" << s
            << "has_error=" << parser.hasError()
            << " is_finished=" << parser.isFinished()
            << " total=" << tmp.size()
            << " content_length=" << parser.getContentLength();
    tmp.resize(tmp.size() - s); //原来的长度减去解析的长度
    SYLAR_LOG_INFO(g_logger) << parser.getData()->toString();
    SYLAR_LOG_INFO(g_logger) << tmp;
}



const char test_response_data[] = "HTTP/1.1 200 OK\r\n"
        "Date: Tue, 04 Jun 2019 15:43:56 GMT\r\n"
        "Server: Apache\r\n"
        "Last-Modified: Tue, 12 Jan 2010 13:48:00 GMT\r\n"
        "ETag: \"51-47cf7e6ee8400\"\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 81\r\n"
        "Cache-Control: max-age=86400\r\n"
        "Expires: Wed, 05 Jun 2019 15:43:56 GMT\r\n"
        "Connection: Close\r\n"
        "Content-Type: text/html\r\n\r\n" //power: head和body之间有一个空行，打印出来空两行是因为日志结束也默认空一行
        "<html>\r\n"
        "<meta http-equiv=\"refresh\" content=\"0;url=http://www.baidu.com/\">\r\n"
        "</html>\r\n";

void test_response(){
    bin::http::HttpResponseParser parser;
    std::string tmp = test_response_data;
    size_t s = parser.execute(&tmp[0], tmp.size(), true);
    SYLAR_LOG_ERROR(g_logger) << "execute rt=" << s
            << " has_error=" << parser.hasError()
            << " is_finished=" << parser.isFinished()
            << " total=" << tmp.size()
            << " content_length=" << parser.getContentLength()
            << " tmp[s]=" << tmp[s];

    tmp.resize(tmp.size() - s);

    SYLAR_LOG_INFO(g_logger) << parser.getData()->toString();
    SYLAR_LOG_INFO(g_logger) << tmp;
}

int main(int argc, char** argv){
    test_request();
    SYLAR_LOG_INFO(g_logger) << "--------------";
    test_response();
    return 0;
}
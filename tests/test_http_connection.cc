#include <iostream>
#include "IOCoroutineScheduler/http/http_connection.h"
#include "IOCoroutineScheduler/log.h"
#include "IOCoroutineScheduler/iomanager.h"
#include "IOCoroutineScheduler/http/http_parser.h"
//#include "IOCoroutineScheduler/streams/zlib_stream.h"
#include <fstream>


static bin::Logger::ptr g_logger = BIN_LOG_ROOT();
static bin::Logger::ptr g_logger2 = BIN_LOG_NAME("connect");


void test_pool(){
    bin::http::HttpConnectionPool::ptr pool(new bin::http::HttpConnectionPool(
                "www.bin.top", "", 80, false, 10, 1000 * 30, 5));

    bin::IOManager::GetThis()->addTimer(1000, [pool](){
            auto r = pool->doGet("/", 300);
            BIN_LOG_INFO(g_logger) << r->toString();
    }, true);
}


//block2: 测试由服务器主动发起连接，是否能够正常接收chunck/非chunck响应报文
void run(){
    //添加一个往文件输出的日志器
    g_logger2->addAppender(bin::LogAppender::ptr(new bin::FileLogAppender("./connection.txt")));

    //向该网站发起请求
    bin::Address::ptr addr = bin::Address::LookupAnyIPAddress("www.bin.top:80");
    if(!addr){
        BIN_LOG_INFO(g_logger) << "get addr error";
        return;
    }

    bin::Socket::ptr sock = bin::Socket::CreateTCP(addr);
    bool rt = sock->connect(addr);
    if(!rt){
        BIN_LOG_INFO(g_logger) << "connect " << *addr << " failed";
        return;
    }

    bin::http::HttpConnection::ptr conn(new bin::http::HttpConnection(sock));
    bin::http::HttpRequest::ptr req(new bin::http::HttpRequest);
    req->setPath("/blog/"); //浏览器访问www.bin.top，会加路径定向到www.bin.top/blog/
    req->setHeader("host", "www.bin.top");
    BIN_LOG_INFO(g_logger) << "req:" << std::endl
        << *req;

    //发送请求接受响应
    conn->sendRequest(req);
    auto rsp = conn->recvResponse();

    if(!rsp){
        BIN_LOG_INFO(g_logger) << "recv response error";
        return;
    }
    // BIN_LOG_INFO(g_logger) << "rsp:" << std::endl
    //     << *rsp;
    //响应报文太长，没打印日志直接保存到rsp.dat文件中
    std::ofstream ofs("rsp.dat");
    ofs << *rsp;

    BIN_LOG_INFO(g_logger) << "=========================";

    auto r = bin::http::HttpConnection::DoGet("http://www.bin.top/blog/", 300);
    BIN_LOG_INFO(g_logger) << "result=" << r->result
        << " error=" << r->error
        << " rsp=" << (r->response ? r->response->toString() : "");

    BIN_LOG_INFO(g_logger) << "=========================";
    test_pool();
}

/*
// void test_https(){
//     auto r = bin::http::HttpConnection::DoGet("http://www.baidu.com/", 300, {
//                         {"Accept-Encoding", "gzip, deflate, br"},
//                         {"Connection", "keep-alive"},
//                         {"User-Agent", "curl/7.29.0"}
//             });
//     BIN_LOG_INFO(g_logger) << "result=" << r->result
//         << " error=" << r->error
//         << " rsp=" << (r->response ? r->response->toString() : "");
//
//     //bin::http::HttpConnectionPool::ptr pool(new bin::http::HttpConnectionPool(
//     //            "www.baidu.com", "", 80, false, 10, 1000 * 30, 5));
//     auto pool = bin::http::HttpConnectionPool::Create(
//                     "https://www.baidu.com", "", 10, 1000 * 30, 5);
//     bin::IOManager::GetThis()->addTimer(1000, [pool](){
//             auto r = pool->doGet("/", 3000, {
//                         {"Accept-Encoding", "gzip, deflate, br"},
//                         {"User-Agent", "curl/7.29.0"}
//                     });
//             BIN_LOG_INFO(g_logger) << r->toString();
//     }, true);
// }

// void test_data(){
//     bin::Address::ptr addr = bin::Address::LookupAny("www.baidu.com:80");
//     auto sock = bin::Socket::CreateTCP(addr);
//
//     sock->connect(addr);
//     const char buff[] = "GET / HTTP/1.1\r\n"
//                 "connection: close\r\n"
//                 "Accept-Encoding: gzip, deflate, br\r\n"
//                 "Host: www.baidu.com\r\n\r\n";
//     sock->send(buff, sizeof(buff));

//     std::string line;
//     line.resize(1024);

//     std::ofstream ofs("http.dat", std::ios::binary);
//     int total = 0;
//     int len = 0;
//     while((len = sock->recv(&line[0], line.size())) > 0){
//         total += len;
//         ofs.write(line.c_str(), len);
//     }
//     std::cout << "total: " << total << " tellp=" << ofs.tellp() << std::endl;
//     ofs.flush();
// }

// void test_parser(){
//     std::ifstream ifs("http.dat", std::ios::binary);
//     std::string content;
//     std::string line;
//     line.resize(1024);
//
//     int total = 0;
//     while(!ifs.eof()){
//         ifs.read(&line[0], line.size());
//         content.append(&line[0], ifs.gcount());
//         total += ifs.gcount();
//     }
//
//     std::cout << "length: " << content.size() << " total: " << total << std::endl;
//     bin::http::HttpResponseParser parser;
//     size_t nparse = parser.execute(&content[0], content.size(), false);
//     std::cout << "finish: " << parser.isFinished() << std::endl;
//     content.resize(content.size() - nparse);
//     std::cout << "rsp: " << *parser.getData() << std::endl;
//
//     auto& client_parser = parser.getParser();
//     std::string body;
//     int cl = 0;
//     do {
//         size_t nparse = parser.execute(&content[0], content.size(), true);
//         std::cout << "content_len: " << client_parser.content_len
//                   << " left: " << content.size()
//                   << std::endl;
//         cl += client_parser.content_len;
//         content.resize(content.size() - nparse);
//         body.append(content.c_str(), client_parser.content_len);
//         content = content.substr(client_parser.content_len + 2);
//     } while(!client_parser.chunks_done);
//
//     std::cout << "total: " << body.size() << " content:" << cl << std::endl;
//
//     //todo:
//     // bin::ZlibStream::ptr stream = bin::ZlibStream::CreateGzip(false);
//     // stream->write(body.c_str(), body.size());
//     // stream->flush();
//
//     // body = stream->getResult();
//
//     // std::ofstream ofs("http.txt");
//     // ofs << body;
// }
*/

int main(int argc, char** argv){
    bin::IOManager iom(2);
    // iom.schedule(run);
    //iom.schedule(test_https);
    run();
    return 0;
}

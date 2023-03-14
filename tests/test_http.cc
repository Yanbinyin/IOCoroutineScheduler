#include "server-bin/http/http.h"
#include "server-bin/log.h"

void test_request(){
    sylar::http::HttpRequest::ptr req(new sylar::http::HttpRequest);
    req->setHeader("host" , "www.sylar.top");
    req->setBody("hello sylar");
    req->dump(std::cout) << std::endl;
}

void test_response(){
    sylar::http::HttpResponse::ptr rsp(new sylar::http::HttpResponse);
    rsp->setHeader("X-X", "server-bin");
    rsp->setBody("hello sylar");
    rsp->setStatus((sylar::http::HttpStatus)400);   //枚举类要强制类型转换，不然报错
    rsp->setClose(false);

    rsp->dump(std::cout) << std::endl;
}

int main(int argc, char** argv){
    test_request();
    std::cout << "---" << std::endl;
    test_response();
    return 0;
}
#include "IOCoroutineScheduler/http/http.h"
#include "IOCoroutineScheduler/log.h"

void test_request(){
    bin::http::HttpRequest::ptr req(new bin::http::HttpRequest);
    req->setHeader("host" , "www.bin.top");
    req->setBody("hello bin");
    req->dump(std::cout) << std::endl;
}

void test_response(){
    bin::http::HttpResponse::ptr rsp(new bin::http::HttpResponse);
    rsp->setHeader("X-X", "IOCoroutineScheduler");
    rsp->setBody("hello bin");
    rsp->setStatus((bin::http::HttpStatus)400);   //枚举类要强制类型转换，不然报错
    rsp->setClose(false);

    rsp->dump(std::cout) << std::endl;
}

int main(int argc, char** argv){
    test_request();
    std::cout << "---" << std::endl;
    test_response();
    return 0;
}
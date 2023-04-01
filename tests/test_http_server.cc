#include "IOCoroutineScheduler/http/http_server.h"
#include "IOCoroutineScheduler/log.h"



static bin::Logger::ptr g_logger = BIN_LOG_ROOT();

#define XX(...) #__VA_ARGS__

/*
查看防火墙状态：sudo systemctl status firewall
关闭防火墙：sudo systemctl stop firewall
*/

bin::IOManager::ptr worker;
void run(){
    g_logger->setLevel(bin::LogLevel::INFO);
    //bin::http::HttpServer::ptr server(new bin::http::HttpServer(true, worker.get(), bin::IOManager::GetThis()));
    bin::http::HttpServer::ptr server(new bin::http::HttpServer(true));
    bin::Address::ptr addr = bin::Address::LookupAnyIPAddress("0.0.0.0:8020");
    while(!server->bind(addr))
        sleep(2);
    
    //获取Servlet管理器对象
    auto sd = server->getServletDispatch();
    //添加一个精确匹配的 服务
    sd->addServlet("/bin/xx", [](bin::http::HttpRequest::ptr req
                ,bin::http::HttpResponse::ptr rsp
                ,bin::http::HttpSession::ptr session){
            rsp->setBody(req->toString()); //响应报文实体 置为请求报文
            return 0;
    });

    sd->addGlobServlet("/bin/*", [](bin::http::HttpRequest::ptr req
                ,bin::http::HttpResponse::ptr rsp
                ,bin::http::HttpSession::ptr session){
            rsp->setBody("Glob:\r\n" + req->toString());
            return 0;
    });

    sd->addGlobServlet("/binx/*", [](bin::http::HttpRequest::ptr req
                ,bin::http::HttpResponse::ptr rsp
                ,bin::http::HttpSession::ptr session){
            rsp->setBody(XX(<html>
<head><title>404 404 Not Found</title></head>
<body>
<center><h1>404 404 Not Found</h1></center>
<hr><center>nginx/1.16.0</center>
</body>
</html>
<!-- a padding to disable MSIE and Chrome friendly error page -->
<!-- a padding to disable MSIE and Chrome friendly error page -->
<!-- a padding to disable MSIE and Chrome friendly error page -->
<!-- a padding to disable MSIE and Chrome friendly error page -->
<!-- a padding to disable MSIE and Chrome friendly error page -->
<!-- a padding to disable MSIE and Chrome friendly error page -->
));
            return 0;
    });

    server->start();
}

void simpleRun(){
    bin::http::HttpServer::ptr server(new bin::http::HttpServer);
    bin::Address::ptr addr = bin::Address::LookupAnyIPAddress("0.0.0.0:8888");
    while(!server->bind(addr)){
        sleep(2);
    }
    auto sd = server->getServletDispatch();
    
    // auto cb = [](bin::http::HttpRequest::ptr req,bin::http::HttpResponse::ptr rsp,bin::http::HttpSession::ptr session){
    //             rsp->setBody(req->toString());
    //             return 0;
    // };
    //浏览器输入http://192.168.26.128:8888/bin/xx 匹配这里
    sd->addServlet("/bin/xx", [](bin::http::HttpRequest::ptr req
                ,bin::http::HttpResponse::ptr rsp
                ,bin::http::HttpSession::ptr session){
        rsp->setBody(req->toString());
        return 0;
    });

    //浏览器输入http://192.168.26.128:8888/bin/* 模糊匹配这里
    sd->addGlobServlet("/bin/*", [](bin::http::HttpRequest::ptr req
                ,bin::http::HttpResponse::ptr rsp
                ,bin::http::HttpSession::ptr session){
        rsp->setBody("Glob:\r\n" + req->toString());
        return 0;
    });
    server->start();
}
int main(int argc, char** argv){
    bin::IOManager iom(1, true, "main");
    worker.reset(new bin::IOManager(3, false, "worker"));
    //iom.schedule(run);
    iom.schedule(simpleRun);
    return 0;
}

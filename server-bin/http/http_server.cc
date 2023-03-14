#include "http_server.h"
#include "server-bin/log.h"

// //todo:
// #include "server-bin/http/servlets/config_servlet.h"
// #include "server-bin/http/servlets/status_servlet.h"

namespace sylar::http {
//namespace http {

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    HttpServer::HttpServer(bool keepalive
                    ,sylar::IOManager* worker
                    ,sylar::IOManager* io_worker
                    ,sylar::IOManager* accept_worker)
            :TcpServer(worker, io_worker, accept_worker)
            ,m_isKeepalive(keepalive){
        m_dispatch.reset(new ServletDispatch);

        m_type = "http";
        // //todo:
        //m_dispatch->addServlet("/_/status", Servlet::ptr(new StatusServlet));
        //m_dispatch->addServlet("/_/config", Servlet::ptr(new ConfigServlet));
    }

    void HttpServer::setName(const std::string& v){
        TcpServer::setName(v);
        m_dispatch->setDefault(std::make_shared<NotFoundServlet>(v));
    }

    //处理已经连接的客户端  完成数据交互通信
    void HttpServer::handleClient(Socket::ptr client){
        SYLAR_LOG_DEBUG(g_logger) << "handleClient " << *client;
        HttpSession::ptr session(new HttpSession(client));
        //power:长连接只要req不关闭，do while将一直循环
        do{
            //接收请求报文
            auto req = session->recvRequest();
            if(!req){
                SYLAR_LOG_DEBUG(g_logger) << "recv http request fail, errno=" << errno << " errstr=" << strerror(errno)
                        << " cliet:" << *client << " keep_alive=" << m_isKeepalive;
                break;
            }

            //回复响应报文
            HttpResponse::ptr rsp(new HttpResponse(req->getVersion(), req->isClose() || !m_isKeepalive)); //请求被关闭 或者 不支持长连接就关闭
            rsp->setHeader("Server", getName());

            //handle里面不直接sendResponse，因为有时Servlet需要多层处理，每一层都要往里面添加东西，而且可能handle之前之后都要添加东西
            //因此handle填充response，session做上下文的判断
            m_dispatch->handle(req, rsp, session);
            
            //取消注释显示请求和响应报文的内容
            SYLAR_LOG_INFO(g_logger) << "request:" << std::endl << *req;
            SYLAR_LOG_INFO(g_logger) << "response:" << std::endl << *rsp;
            
            session->sendResponse(rsp);
            if(!m_isKeepalive || req->isClose()){
                break;
            }
        }while(true);
        session->close();
    }

//}
}

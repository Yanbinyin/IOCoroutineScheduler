#ifndef __HTTP_WS_SERVLET_H__
#define __HTTP_WS_SERVLET_H__

#include "ws_session.h"
#include "IOCoroutineScheduler/thread.h"
#include "servlet.h"

namespace bin {
namespace http {

class WSServlet : public Servlet {
public:
    typedef std::shared_ptr<WSServlet> ptr;
    WSServlet(const std::string& name)
        :Servlet(name){
    }
    virtual ~WSServlet(){}

    virtual int32_t handle(bin::http::HttpRequest::ptr request
                   , bin::http::HttpResponse::ptr response
                   , bin::http::HttpSession::ptr session) override {
        return 0;
    }

    virtual int32_t onConnect(bin::http::HttpRequest::ptr header
                              ,bin::http::WSSession::ptr session) = 0;
    virtual int32_t onClose(bin::http::HttpRequest::ptr header
                             ,bin::http::WSSession::ptr session) = 0;
    virtual int32_t handle(bin::http::HttpRequest::ptr header
                           ,bin::http::WSFrameMessage::ptr msg
                           ,bin::http::WSSession::ptr session) = 0;
    const std::string& getName() const { return m_name;}
protected:
    std::string m_name;
};

class FunctionWSServlet : public WSServlet {
public:
    typedef std::shared_ptr<FunctionWSServlet> ptr;
    typedef std::function<int32_t (bin::http::HttpRequest::ptr header
                              ,bin::http::WSSession::ptr session)> on_connect_cb;
    typedef std::function<int32_t (bin::http::HttpRequest::ptr header
                             ,bin::http::WSSession::ptr session)> on_close_cb; 
    typedef std::function<int32_t (bin::http::HttpRequest::ptr header
                           ,bin::http::WSFrameMessage::ptr msg
                           ,bin::http::WSSession::ptr session)> callback;

    FunctionWSServlet(callback cb
                      ,on_connect_cb connect_cb = nullptr
                      ,on_close_cb close_cb = nullptr);

    virtual int32_t onConnect(bin::http::HttpRequest::ptr header
                              ,bin::http::WSSession::ptr session) override;
    virtual int32_t onClose(bin::http::HttpRequest::ptr header
                             ,bin::http::WSSession::ptr session) override;
    virtual int32_t handle(bin::http::HttpRequest::ptr header
                           ,bin::http::WSFrameMessage::ptr msg
                           ,bin::http::WSSession::ptr session) override;
protected:
    callback m_callback;
    on_connect_cb m_onConnect;
    on_close_cb m_onClose;
};

class WSServletDispatch : public ServletDispatch {
public:
    typedef std::shared_ptr<WSServletDispatch> ptr;
    typedef RWMutex RWMutexType;

    WSServletDispatch();
    void addServlet(const std::string& uri
                    ,FunctionWSServlet::callback cb
                    ,FunctionWSServlet::on_connect_cb connect_cb = nullptr
                    ,FunctionWSServlet::on_close_cb close_cb = nullptr);
    void addGlobServlet(const std::string& uri
                    ,FunctionWSServlet::callback cb
                    ,FunctionWSServlet::on_connect_cb connect_cb = nullptr
                    ,FunctionWSServlet::on_close_cb close_cb = nullptr);
    WSServlet::ptr getWSServlet(const std::string& uri);
};

}
}

#endif

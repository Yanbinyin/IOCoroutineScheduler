#ifndef __SYLAR_HTTP_SERVLETS_CONFIG_SERVLET_H__
#define __SYLAR_HTTP_SERVLETS_CONFIG_SERVLET_H__

#include "server-bin/http/servlet.h"

namespace sylar {
namespace http {

class ConfigServlet : public Servlet {
public:
    ConfigServlet();
    virtual int32_t handle(sylar::http::HttpRequest::ptr request
                   , sylar::http::HttpResponse::ptr response
                   , sylar::http::HttpSession::ptr session) override;
};

}
}

#endif

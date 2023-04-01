#ifndef __SYLAR_HTTP_SERVLETS_STATUS_SERVLET_H__
#define __SYLAR_HTTP_SERVLETS_STATUS_SERVLET_H__

#include "IOCoroutineScheduler/http/servlet.h"

namespace bin {
namespace http {

class StatusServlet : public Servlet {
public:
    StatusServlet();
    virtual int32_t handle(bin::http::HttpRequest::ptr request
                   , bin::http::HttpResponse::ptr response
                   , bin::http::HttpSession::ptr session) override;
};

}
}

#endif

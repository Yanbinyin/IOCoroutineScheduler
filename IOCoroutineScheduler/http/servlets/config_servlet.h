#ifndef __BIN_HTTP_SERVLETS_CONFIG_SERVLET_H__
#define __BIN_HTTP_SERVLETS_CONFIG_SERVLET_H__

#include "IOCoroutineScheduler/http/servlet.h"

namespace bin {
namespace http {

class ConfigServlet : public Servlet {
public:
    ConfigServlet();
    virtual int32_t handle(bin::http::HttpRequest::ptr request
                   , bin::http::HttpResponse::ptr response
                   , bin::http::HttpSession::ptr session) override;
};

}
}

#endif

#ifndef __SYLAR_HTTP_WS_SERVER_H__
#define __SYLAR_HTTP_WS_SERVER_H__

#include "IOCoroutineScheduler/tcp_server.h"
#include "ws_session.h"
#include "ws_servlet.h"

namespace bin {
namespace http {

class WSServer : public TcpServer {
public:
    typedef std::shared_ptr<WSServer> ptr;

    WSServer(bin::IOManager* worker = bin::IOManager::GetThis()
             , bin::IOManager* io_worker = bin::IOManager::GetThis()
             , bin::IOManager* accept_worker = bin::IOManager::GetThis());

    WSServletDispatch::ptr getWSServletDispatch() const { return m_dispatch;}
    void setWSServletDispatch(WSServletDispatch::ptr v){ m_dispatch = v;}
protected:
    virtual void handleClient(Socket::ptr client) override;
protected:
    WSServletDispatch::ptr m_dispatch;
};

}
}

#endif

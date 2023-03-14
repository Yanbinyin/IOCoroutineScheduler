#include "servlet.h"
#include <fnmatch.h>

namespace sylar::http {
//namespace http {
    //block1:FunctionServlet
    FunctionServlet::FunctionServlet(callback cb)
        :Servlet("FunctionServlet")
        ,m_cb(cb){
    }

    int32_t FunctionServlet::handle(sylar::http::HttpRequest::ptr request
                , sylar::http::HttpResponse::ptr response
                , sylar::http::HttpSession::ptr session){
        return m_cb(request, response, session);
    }



    //block2:ServletDispatch

    ServletDispatch::ServletDispatch()
        :Servlet("ServletDispatch"){
        m_default.reset(new NotFoundServlet("server-bin/1.0"));
    }

    /*ServletDispatch继承后，负责管理所有的服务对象，支持精准资源请求、模糊资源请求，通过handle()去执行分管的子服务的回调函数。
    当一个请求到达服务器端，优先精准匹配符合的请求资源，如果没有就去检索有可能符合的请求资源。
    都没有找到相对应的资源，将使用一个默认的服务来处理这个可能不太合法的请求。*/
    int32_t ServletDispatch::handle(sylar::http::HttpRequest::ptr request
                , sylar::http::HttpResponse::ptr response
                , sylar::http::HttpSession::ptr session){
        auto slt = getMatchedServlet(request->getPath());
        if(slt){
            slt->handle(request, response, session);
        }
        return 0;
    }

    void ServletDispatch::addServlet(const std::string& uri, Servlet::ptr slt){
        RWMutexType::WriteLock lock(m_mutex);
        m_datas[uri] = std::make_shared<HoldServletCreator>(slt);
    }

    void ServletDispatch::addServletCreator(const std::string& uri, IServletCreator::ptr creator){
        RWMutexType::WriteLock lock(m_mutex);
        m_datas[uri] = creator;
    }

    void ServletDispatch::addGlobServletCreator(const std::string& uri, IServletCreator::ptr creator){
        RWMutexType::WriteLock lock(m_mutex);
        for(auto it = m_globs.begin(); it != m_globs.end(); ++it){
            if(it->first == uri){
                m_globs.erase(it);
                break;
            }
        }
        m_globs.push_back(std::make_pair(uri, creator));
    }

    void ServletDispatch::addServlet(const std::string& uri, FunctionServlet::callback cb){
        RWMutexType::WriteLock lock(m_mutex);
        m_datas[uri] = std::make_shared<HoldServletCreator>(
                            std::make_shared<FunctionServlet>(cb));
    }

    void ServletDispatch::addGlobServlet(const std::string& uri
                                        ,Servlet::ptr slt){
        RWMutexType::WriteLock lock(m_mutex);
        for(auto it = m_globs.begin();
                it != m_globs.end(); ++it){
            if(it->first == uri){
                m_globs.erase(it);
                break;
            }
        }
        m_globs.push_back(std::make_pair(uri
                    , std::make_shared<HoldServletCreator>(slt)));
    }

    void ServletDispatch::addGlobServlet(const std::string& uri, FunctionServlet::callback cb){
        return addGlobServlet(uri, std::make_shared<FunctionServlet>(cb));
    }

    void ServletDispatch::delServlet(const std::string& uri){
        RWMutexType::WriteLock lock(m_mutex);
        m_datas.erase(uri);
    }

    void ServletDispatch::delGlobServlet(const std::string& uri){
        RWMutexType::WriteLock lock(m_mutex);
        for(auto it = m_globs.begin();
                it != m_globs.end(); ++it){
            if(it->first == uri){
                m_globs.erase(it);
                break;
            }
        }
    }

    Servlet::ptr ServletDispatch::getServlet(const std::string& uri){
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_datas.find(uri);
        return it == m_datas.end() ? nullptr : it->second->get();
    }

    Servlet::ptr ServletDispatch::getGlobServlet(const std::string& uri){
        RWMutexType::ReadLock lock(m_mutex);
        for(auto it = m_globs.begin();
                it != m_globs.end(); ++it){
            if(it->first == uri){
                return it->second->get();
            }
        }
        return nullptr;
    }

    Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& uri){
        RWMutexType::ReadLock lock(m_mutex);
        auto mit = m_datas.find(uri);
        if(mit != m_datas.end()){
            return mit->second->get();
        }
        for(auto it = m_globs.begin();
                it != m_globs.end(); ++it){
            if(!fnmatch(it->first.c_str(), uri.c_str(), 0)){
                return it->second->get();
            }
        }
        return m_default;
    }

    void ServletDispatch::listAllServletCreator(std::map<std::string, IServletCreator::ptr>& infos){
        RWMutexType::ReadLock lock(m_mutex);
        for(auto& i : m_datas){
            infos[i.first] = i.second;
        }
    }

    void ServletDispatch::listAllGlobServletCreator(std::map<std::string, IServletCreator::ptr>& infos){
        RWMutexType::ReadLock lock(m_mutex);
        for(auto& i : m_globs){
            infos[i.first] = i.second;
        }
    }




    NotFoundServlet::NotFoundServlet(const std::string& name)
        :Servlet("NotFoundServlet")
        ,m_name(name){
        m_content = "<html><head><title>404 Not Found"
            "</title></head><body><center><h1>404 Not Found</h1></center>"
            "<hr><center>" + name + "</center></body></html>";

    }

    int32_t NotFoundServlet::handle(sylar::http::HttpRequest::ptr request
                    , sylar::http::HttpResponse::ptr response
                    , sylar::http::HttpSession::ptr session){
        response->setStatus(sylar::http::HttpStatus::NOT_FOUND);
        response->setHeader("Server", "server-bin/1.0.0");
        response->setHeader("Content-Type", "text/html");
        response->setBody(m_content);
        return 0;
    }


//}
}

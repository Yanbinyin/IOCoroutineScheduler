//Servlet封装

#ifndef __SYLAR_HTTP_SERVLET_H__
#define __SYLAR_HTTP_SERVLET_H__

#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include "http.h"
#include "http_session.h"
#include "IOCoroutineScheduler/thread.h"
#include "IOCoroutineScheduler/util.h"

namespace bin::http {
//namespace http {

    /*Servlet是Server Applet的一个简称，直译就是服务小程序的意思。该概念常见于Java中。
    一般而言，B/S模型（浏览器-服务器模型），通过浏览器发送访问请求，服务器接收请求，并对这些请求作出处理。
    而servlet就是专门对请求作出处理的组件*/
    class Servlet {
    public:
        typedef std::shared_ptr<Servlet> ptr;
        
        Servlet(const std::string& name)//name 名称
            :m_name(name){}

        virtual ~Servlet(){}

        //处理请求 request HTTP请求 response HTTP响应 session HTTP连接
        //通过回调函数的形式，存储相应的请求所要做的处理动作，通过handel()去执行该回调函数
        virtual int32_t handle(bin::http::HttpRequest::ptr request
                    , bin::http::HttpResponse::ptr response
                    , bin::http::HttpSession::ptr session) = 0;

        const std::string& getName() const { return m_name;}    //返回Servlet名称

    protected:
        std::string m_name; //服务名称
    };



    //函数式Servlet，接收一个Function的参数，可以通过Function而非继承的方式才能支持Servlet
    class FunctionServlet : public Servlet {
    public:
        typedef std::shared_ptr<FunctionServlet> ptr;

        //函数回调类型定义//hack:
        typedef std::function<int32_t (bin::http::HttpRequest::ptr request
                    , bin::http::HttpResponse::ptr response
                    , bin::http::HttpSession::ptr session)> callback;


        //@param[in] cb 回调函数
        FunctionServlet(callback cb);
        virtual int32_t handle(bin::http::HttpRequest::ptr request
                    , bin::http::HttpResponse::ptr response
                    , bin::http::HttpSession::ptr session) override;

    private:
        
        callback m_cb;  //回调函数
    };



    //hack: 不懂是做什么的类
    class IServletCreator {
    public:
        typedef std::shared_ptr<IServletCreator> ptr;
        virtual ~IServletCreator(){}
        virtual Servlet::ptr get() const = 0;
        virtual std::string getName() const = 0;
    };

    class HoldServletCreator : public IServletCreator {
    public:
        typedef std::shared_ptr<HoldServletCreator> ptr;
        HoldServletCreator(Servlet::ptr slt)
            :m_servlet(slt){
        }

        Servlet::ptr get() const override {
            return m_servlet;
        }

        std::string getName() const override {
            return m_servlet->getName();
        }
    private:
        Servlet::ptr m_servlet;
    };

    template<class T>
    class ServletCreator : public IServletCreator {
    public:
        typedef std::shared_ptr<ServletCreator> ptr;

        ServletCreator(){
        }

        Servlet::ptr get() const override {
            return Servlet::ptr(new T);
        }

        std::string getName() const override {
            return TypeToName<T>();
        }
    };




    //Servlet分发器，负责管理所有servlet之间的关系
    /*  ● 使用unordered_map容器存储精准匹配的服务对象，不允许同一个资源有重复的处理。
        ● 使用vector<pair< > >组合容器存储模糊匹配的服务对象，允许同一个资源有重复的处理。

        ● 由于服务也属于一种公共资源，应考虑线程安全的问题，对服务的管理属于一种"读多写少"的场景，考虑使用读写锁。*/
    class ServletDispatch : public Servlet {
    public:
        typedef std::shared_ptr<ServletDispatch> ptr;   //智能指针类型定义
        typedef RWMutex RWMutexType;                    //读写锁类型定义

        ServletDispatch();

        //核心函数：服务执行
        virtual int32_t handle(bin::http::HttpRequest::ptr request
                    , bin::http::HttpResponse::ptr response
                    , bin::http::HttpSession::ptr session) override;

        void addServlet(const std::string& uri, Servlet::ptr slt);              //添加servlet uri uri slt serlvet

        void addServlet(const std::string& uri, FunctionServlet::callback cb);  //添加servlet

        void addGlobServlet(const std::string& uri, Servlet::ptr slt);              //添加模糊匹配servlet uri uri 模糊匹配 /bin_*
        void addGlobServlet(const std::string& uri, FunctionServlet::callback cb);  //添加模糊匹配servlet uri uri 模糊匹配 /bin_*

        void addServletCreator(const std::string& uri, IServletCreator::ptr creator);
        void addGlobServletCreator(const std::string& uri, IServletCreator::ptr creator);

        template<class T>
        void addServletCreator(const std::string& uri){
            addServletCreator(uri, std::make_shared<ServletCreator<T> >());
        }

        template<class T>
        void addGlobServletCreator(const std::string& uri){
            addGlobServletCreator(uri, std::make_shared<ServletCreator<T> >());
        }

        void delServlet(const std::string& uri);                //删除精准匹配 servlet uri uri
        void delGlobServlet(const std::string& uri);            //删除模糊匹配 servlet uri uri

        Servlet::ptr getDefault() const { return m_default;}    //返回默认servlet
        void setDefault(Servlet::ptr v){ m_default = v;}       //设置默认servlet v servlet

        Servlet::ptr getServlet(const std::string& uri);        //通过uri获取servlet,返回对应的servlet
        Servlet::ptr getGlobServlet(const std::string& uri);    //通过uri获取模糊匹配servlet,返回对应的servlet
        Servlet::ptr getMatchedServlet(const std::string& uri); //通过uri获取servlet,优先精准匹配,其次模糊匹配,最后返回默认

        void listAllServletCreator(std::map<std::string, IServletCreator::ptr>& infos);
        void listAllGlobServletCreator(std::map<std::string, IServletCreator::ptr>& infos);

    private:
        //精准匹配servlet MAP
        //uri(/bin/xxx) -> servlet
        std::unordered_map<std::string, IServletCreator::ptr> m_datas;
        //模糊匹配servlet 数组
        //uri(/bin/*) -> servlet
        std::vector<std::pair<std::string, IServletCreator::ptr> > m_globs;
        Servlet::ptr m_default; //默认servlet，所有路径都没匹配到时使用
        RWMutexType m_mutex;    //读写互斥量
    };



    //NotFoundServlet，(默认返回404)。当访问的URL不存在，默认放回NotFoundServlet
    class NotFoundServlet : public Servlet {
    public:
        typedef std::shared_ptr<NotFoundServlet> ptr;

        NotFoundServlet(const std::string& name);
        virtual int32_t handle(bin::http::HttpRequest::ptr request
                    , bin::http::HttpResponse::ptr response
                    , bin::http::HttpSession::ptr session) override;

    private:
        std::string m_name;
        std::string m_content;
    };

//}
}

#endif

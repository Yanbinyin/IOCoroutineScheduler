//URI封装类: 封装对远端服务器发起请求后的HTTP报文处理结果

#ifndef __SYLAR_URI_H__
#define __SYLAR_URI_H__

#include <memory>
#include <string>
#include <stdint.h>
#include "address.h"

namespace sylar {
    /*
    The generic URI syntax consists of a hierarchical sequence of
    components referred to as the scheme, authority, path, query, and
    fragment.

        URI         = scheme ":" hier-part [ "?" query ] [ "#" fragment ]

        hier-part   = "//" authority path-abempty
                    / path-absolute
                    / path-rootless
                    / path-empty

    The scheme and path components are required, though the path may be
    empty (no characters).  When authority is present, the path must
    either be empty or begin with a slash ("/") character.  When
    authority is not present, the path cannot begin with two slash
    characters ("//").  These restrictions result in five different ABNF
    rules for a path (Section 3.3), only one of which will match any
    given URI reference.

    The following are two example URIs and their component parts:

            foo://example.com:8042/over/there?name=ferret#nose
            \_/   \______________/\_________/ \_________/ \__/
            |           |            |            |        |
        scheme     authority       path        query   fragment
            |   _____________________|__
            / \ /                        \
            urn:example:animal:ferret:nose
    */

    //URI类: 解析用ragel来做。类中的函数要放在uri.rl中去实现 //power:需要理解ragel的语法规则、正则表达式规则
    class Uri {
    public:
        typedef std::shared_ptr<Uri> ptr;

        static Uri::ptr Create(const std::string& uri); //通过字符串uri创建Uri对象,解析成功返回Uri对象否则返回nullptr

        Uri();

        const std::string& getScheme() const { return m_scheme;}        //返回scheme
        const std::string& getUserinfo() const { return m_userinfo;}    //返回用户信息
        const std::string& getHost() const { return m_host;}            //返回host
        const std::string& getPath() const;                             //返回路径
        const std::string& getQuery() const { return m_query;}          //返回查询条件
        const std::string& getFragment() const { return m_fragment;}    //返回fragment
        int32_t getPort() const;                                        //返回端口

        void setScheme(const std::string& v){ m_scheme = v;}       //设置scheme v scheme
        void setUserinfo(const std::string& v){ m_userinfo = v;}   //设置用户信息 v 用户信息
        void setHost(const std::string& v){ m_host = v;}           //设置host信息 v host
        void setPath(const std::string& v){ m_path = v;}           //设置路径 v 路径
        void setQuery(const std::string& v){ m_query = v;}         //设置查询条件
        void setFragment(const std::string& v){ m_fragment = v;}   //设置fragment v fragment
        void setPort(int32_t v){ m_port = v;}                      //设置端口号 v 端口

        std::ostream& dump(std::ostream& os) const; //核心函数：序列化到输出流 os 输出流
        std::string toString() const;               //转成字符串

        Address::ptr createAddress() const; //创建Address，URI中有host和port

    private:
        bool isDefaultPort() const;          //是否默认端口

    private:        
        std::string m_scheme;   //schema        
        std::string m_userinfo; //用户信息        
        std::string m_host;     //host        
        std::string m_path;     //路径        
        std::string m_query;    //查询参数        
        std::string m_fragment; //fragment
        int32_t m_port;         //端口
    };


}

#endif

//CP服务器的封装

#ifndef __SYLAR_TCP_SERVER_H__
#define __SYLAR_TCP_SERVER_H__

#include <memory>
#include <functional>
#include "address.h"
#include "iomanager.h"
#include "socket.h"
#include "noncopyable.h"
#include "config.h"

namespace bin {
    /*
    //skip: TcpServerConf
    */

    struct TcpServerConf {
        typedef std::shared_ptr<TcpServerConf> ptr;

        std::vector<std::string> address;
        int keepalive = 0;
        int timeout = 1000 * 2 * 60;
        int ssl = 0;
        std::string id;
        //服务器类型，http, ws, rock
        std::string type = "http";
        std::string name;
        std::string cert_file;
        std::string key_file;
        std::string accept_worker;
        std::string io_worker;
        std::string process_worker;
        std::map<std::string, std::string> args;

        bool isValid() const {
            return !address.empty();
        }

        bool operator==(const TcpServerConf& oth) const {
            return address == oth.address
                && keepalive == oth.keepalive
                && timeout == oth.timeout
                && name == oth.name
                && ssl == oth.ssl
                && cert_file == oth.cert_file
                && key_file == oth.key_file
                && accept_worker == oth.accept_worker
                && io_worker == oth.io_worker
                && process_worker == oth.process_worker
                && args == oth.args
                && id == oth.id
                && type == oth.type;
        }
    };

    template<>
    class LexicalCast<std::string, TcpServerConf> {
    public:
        TcpServerConf operator()(const std::string& v){
            YAML::Node node = YAML::Load(v);
            TcpServerConf conf;
            conf.id = node["id"].as<std::string>(conf.id);
            conf.type = node["type"].as<std::string>(conf.type);
            conf.keepalive = node["keepalive"].as<int>(conf.keepalive);
            conf.timeout = node["timeout"].as<int>(conf.timeout);
            conf.name = node["name"].as<std::string>(conf.name);
            conf.ssl = node["ssl"].as<int>(conf.ssl);
            conf.cert_file = node["cert_file"].as<std::string>(conf.cert_file);
            conf.key_file = node["key_file"].as<std::string>(conf.key_file);
            conf.accept_worker = node["accept_worker"].as<std::string>();
            conf.io_worker = node["io_worker"].as<std::string>();
            conf.process_worker = node["process_worker"].as<std::string>();
            conf.args = LexicalCast<std::string
                ,std::map<std::string, std::string> >()(node["args"].as<std::string>(""));
            if(node["address"].IsDefined()){
                for(size_t i = 0; i < node["address"].size(); ++i){
                    conf.address.push_back(node["address"][i].as<std::string>());
                }
            }
            return conf;
        }
    };

    template<>
    class LexicalCast<TcpServerConf, std::string> {
    public:
        std::string operator()(const TcpServerConf& conf){
            YAML::Node node;
            node["id"] = conf.id;
            node["type"] = conf.type;
            node["name"] = conf.name;
            node["keepalive"] = conf.keepalive;
            node["timeout"] = conf.timeout;
            node["ssl"] = conf.ssl;
            node["cert_file"] = conf.cert_file;
            node["key_file"] = conf.key_file;
            node["accept_worker"] = conf.accept_worker;
            node["io_worker"] = conf.io_worker;
            node["process_worker"] = conf.process_worker;
            node["args"] = YAML::Load(LexicalCast<std::map<std::string, std::string>
                , std::string>()(conf.args));
            for(auto& i : conf.address){
                node["address"].push_back(i);
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };



    //TCP服务器封装
    class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable {
    public:
        typedef std::shared_ptr<TcpServer> ptr;

        //worker socket客户端工作的协程调度器 accept_worker 服务器socket执行接收socket连接的协程调度器
        TcpServer(bin::IOManager* worker = bin::IOManager::GetThis()
                ,bin::IOManager* io_woker = bin::IOManager::GetThis()
                ,bin::IOManager* accept_worker = bin::IOManager::GetThis());

        virtual ~TcpServer();

        virtual bool bind(bin::Address::ptr addr, bool ssl = false);  //核心函数：绑定地址，返回是否绑定成功
        //核心函数：绑定地址数组bind()+listen() addrs 需要绑定的地址数组，fails 传出绑定失败的地址，是否绑定成功
        virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails, bool ssl = false);
        virtual bool start();   //核心函数：启动服务，需要bind成功后执行
        virtual void stop();    //核心函数：停止服务

        bool loadCertificates(const std::string& cert_file, const std::string& key_file);   //SSL

        uint64_t getRecvTimeout() const { return m_recvTimeout;}    //返回读取超时时间(毫秒)
        std::string getName() const { return m_name;}               //返回服务器名称
        void setRecvTimeout(uint64_t v){ m_recvTimeout = v;}       //设置读取超时时间(毫秒)
        virtual void setName(const std::string& v){ m_name = v;}   //设置服务器名称
        bool isStop() const { return m_isStop;}                     //返回服务器工作状态，是否停止
        TcpServerConf::ptr getConf() const { return m_conf;}
        void setConf(TcpServerConf::ptr v){ m_conf = v;}
        void setConf(const TcpServerConf& v);
        virtual std::string toString(const std::string& prefix = "");
        std::vector<Socket::ptr> getSocks() const { return m_listenSocks;}
    
    protected:
       virtual void startAccept(Socket::ptr sock);      //核心函数：开始接受连接
       virtual void handleClient(Socket::ptr client);   //核心函数：处理新连接的Socket类
    
    protected:
        std::vector<Socket::ptr> m_listenSocks; //监听Socket数组,存储多个监听socket 可能支持多协议 可能存在多个网卡  可能监听多个地址
        
        //新连接的Socket工作的调度器
        IOManager* m_worker;            //暂时无用，保留变量
        IOManager* m_ioWorker;          //作为一个线程池  专门负责已经接收连接(accept)的客户端的调度
        
        IOManager* m_acceptWorker;      //服务器Socket接收连接的调度器
        uint64_t m_recvTimeout;         //接收超时时间(毫秒),防止攻击 ，限制读取时间间接限定数据发送量；防止死的客户端来浪费资源，一段时间没有来自客户端的响应，就要断开与其的连接，类似"心跳"功能。
        std::string m_name;             //服务器名称，socket所属的server也作为一个服务器版本号方便更迭, 有名字可以很方便进行区分是tcp的还是http的sock，也方便日志输出，
        std::string m_type = "tcp";     //服务器类型
        bool m_isStop;                  //服务是否停止
        bool m_ssl = false;
        TcpServerConf::ptr m_conf;
    };

}

#endif

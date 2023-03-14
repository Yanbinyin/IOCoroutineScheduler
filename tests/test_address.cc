#include "server-bin/address.h"
#include "server-bin/log.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

//block: 测试
void test(){
    std::vector<sylar::Address::ptr> addrs;

    bool v = sylar::Address::Lookup(addrs, "localhost:3080");
    //bool v = sylar::Address::Lookup(addrs, "www.baidu.com", AF_INET);
    //bool v = sylar::Address::Lookup(addrs, "www.sylar.top", AF_INET);
    if(!v){
        SYLAR_LOG_ERROR(g_logger) << "lookup fail";
        return;
    }

    for(size_t i = 0; i < addrs.size(); ++i){
        SYLAR_LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
    }

    auto addr = sylar::Address::LookupAny("localhost:4080");
    if(addr){
        SYLAR_LOG_INFO(g_logger) << *addr;
    }else{
        SYLAR_LOG_ERROR(g_logger) << "error";
    }
}

//block2： 检测http、https、ftp
void testFTP(){
    std::vector<sylar::Address::ptr> addrs;

    std::vector<std::string> domainNames;
    domainNames.push_back("www.bilibili.com");
    domainNames.push_back("www.bilibili.com:http");
    domainNames.push_back("www.bilibili.com:https");
    domainNames.push_back("www.bilibili.com:ftp");

    //逐个遍历
    for(auto& dm : domainNames){
        //bool v = sylar::Address::Lookup(addrs, dm, AF_INET, SOCK_STREAM);
        bool v = sylar::Address::Lookup(addrs, dm, AF_INET);
        if(!v){
            SYLAR_LOG_ERROR(g_logger) << "lookup fail";
            return;
        }

        //打印解析获取到的地址
        SYLAR_LOG_INFO(g_logger) << dm;
        for(size_t i = 0; i < addrs.size(); ++i){
            //同样的地址出现了三组，因为返回的协议类型protocol不相同 如果我们指定流式套接字就没有重复了
            //即Address::Lookup()函数里面的type参数
            SYLAR_LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
        }
    }
}

//block3: 是否能通过网卡获取到对应的网址信息
void test_iface(){
    std::multimap<std::string, std::pair<sylar::Address::ptr, uint32_t> > results;

    bool v = sylar::Address::GetInterfaceAddresses(results);
    if(!v){
        SYLAR_LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
        return;
    }

    //将获取到的网址信息 逐一打印
    for(auto& i: results){
        SYLAR_LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - " << i.second.second;
    }
}

void test_createIPv4(){
    //auto addr = sylar::IPAddress::Create("www.sylar.top");
    auto addr = sylar::IPAddress::Create("127.0.0.8");
    if(addr){
        SYLAR_LOG_INFO(g_logger) << addr->toString();
    }
}

int main(int argc, char** argv){
    //test();
    //testFTP();

    //test_createIPv4();
    
    test_iface();
    return 0;
}

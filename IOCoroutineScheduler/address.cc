#include "address.h"
#include "log.h"
#include <sstream>
#include <netdb.h>
#include <ifaddrs.h>
#include <stddef.h>

#include "endian.h"

namespace bin {

    static bin::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    //block: 辅助函数
    //生成一个只有主机号的掩码，即：取反后的子网掩码 bits：掩码的位数
    template<class T>
    static T CreateMask(uint32_t bits){
        return(1 <<(sizeof(T) * 8 - bits)) - 1;
    }

    //计算传入的数value的二进制表示中有多少位"1"。
    template<class T>
    static uint32_t CountBytes(T value){
        uint32_t result = 0;
        for(; value; ++result)
            value &= value - 1; //power:太强了
        
        return result;
    }



    //block： Address类
    //根据传入的协议类型创建对应的通信地址子类对象
    Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen){
        if(addr == nullptr)
            return nullptr;

        Address::ptr result;
        switch(addr->sa_family){
            case AF_INET: //IPv4类型地址
                result.reset(new IPv4Address(*(const sockaddr_in*)addr));
                break;
            case AF_INET6: //IPv6类型地址
                result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
                break;
            // case AF_UNIX: //Unix域类型地址   //bin add
            //     result.reset(new UnixAddress(*(const sockaddr_un*)addr));
            //     break;
            default:
                result.reset(new UnknownAddress(*addr));
                break;
        }
        return result;
    }

    //核心函数：功能：通过一个域名获取所有通信地址（IPv4、IPv6），不支持AF_UNIX。
    //难点：处理IPv6协议，其中包含一个service字段
    bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host, int family, int type, int protocol){
        addrinfo hints, *results, *next;
        hints.ai_flags = 0;
        hints.ai_family = family;
        hints.ai_socktype = type;
        hints.ai_protocol = protocol;
        hints.ai_addrlen = 0;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;

        std::string node;
        const char* service = NULL;

        //1. 检查是否是IPv6形式的请求服务 [xxx]:service
         if(!host.empty() && host[0] == '['){
            //先找到IPv6地址中"]"的位置
            const char* endipv6 =(const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
            if(endipv6){ //如果不为NULL 说明使用一个IPv6的URL
                //TODO check out of range
                //如果"]"后紧跟着一个":"则认为有service字段
                if(*(endipv6 + 1) == ':'){
                    service = endipv6 + 2;//存储service字段的首地址
                }
                //将 [xxx]:service 中xxx的部分取出放入string node中
                node = host.substr(1, endipv6 - host.c_str() - 1);
            }
        }

        //2.如果node为空 说明是IPv4形式请求服务  xxx:service
        if(node.empty()){
            service =(const char*)memchr(host.c_str(), ':', host.size());
            //不为空存在service字段
            if(service){
                if(!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)){
                    node = host.substr(0, service - host.c_str());
                    //此时字符串指针指向":" +1便是service字段起始处
                    ++service;
                }
            }
        }
    
        //3. 如果node在上面的两次检查中都未被赋值 说明不存在service字段 直接赋值
        if(node.empty()){
            node = host;
        }
        //4.调用API获取域名上的网络通信地址
        int error = getaddrinfo(node.c_str(), service, &hints, &results); //libfunc:
        if(error){
            SYLAR_LOG_DEBUG(g_logger) << "Address::Lookup getaddress(" << host << ", "
                << family << ", " << type << ") err=" << error << " errstr="
                << gai_strerror(error);
            return false;
        }

        //5.获取到的所有网络通信地址是一个链表的形式 依次访问构建对应的地址类对象
        next = results;
        while(next){
            result.push_back(Create(next->ai_addr,(socklen_t)next->ai_addrlen));
            //SYLAR_LOG_INFO(g_logger) <<((sockaddr_in*)next->ai_addr)->sin_addr.s_addr;
            next = next->ai_next;
        }

        freeaddrinfo(results); //libfunc:
        return !result.empty();
    }

    Address::ptr Address::LookupAny(const std::string& host, int family, int type, int protocol){
        std::vector<Address::ptr> result;
        if(Lookup(result, host, family, type, protocol)){
            return result[0]; //默认返回得到结果中的第一个地址
        }
        return nullptr;
    }

    IPAddress::ptr Address::LookupAnyIPAddress(const std::string& host, int family, int type, int protocol){
        std::vector<Address::ptr> result;
        if(Lookup(result, host, family, type, protocol)){
            //for(auto& i : result){
            //    std::cout << i->toString() << std::endl;
            //}
            for(auto& i : result){
                IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
                if(v){
                    return v;
                }
            }
        }
        return nullptr;
    }

    //核心函数：通过所有本地网卡获取所有通信地址
    bool Address::GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t> >& result, int family){
        struct ifaddrs *next, *results;
        int ret = getifaddrs(&results); //libfunc:
        if(ret != 0){
            SYLAR_LOG_DEBUG(g_logger) << "Address::GetInterfaceAddresses getifaddrs," //power: std::cout << "str1" "str2"; 中间没有<<
                    " err=" << errno << " errstr=" << strerror(errno);
            return false;
        }

        try {
            for(next = results; next; next = next->ifa_next){
                Address::ptr addr;
                uint32_t prefix_len = ~0u;
                //过滤掉不为指定地址族的结点
                if(family != AF_UNSPEC && family != next->ifa_addr->sa_family){
                    continue;
                }
                switch(next->ifa_addr->sa_family){
                    case AF_INET:
                        {
                            addr = Create(next->ifa_addr, sizeof(sockaddr_in)); //小技巧 掩码转换 sockaddr---->sockaddr_in
                            uint32_t netmask =((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                            prefix_len = CountBytes(netmask); //计算掩码长度
                        }
                        break;
                    case AF_INET6:
                        {
                            addr = Create(next->ifa_addr, sizeof(sockaddr_in6)); //小技巧 掩码转换 sockaddr---->sockaddr_in6
                            in6_addr& netmask =((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                            prefix_len = 0; //计算前缀长度
                            //每一个元素为uint8_t 共有16个
                            for(int i = 0; i < 16; ++i)
                                prefix_len += CountBytes(netmask.s6_addr[i]);
                        }
                        break;
                    default:
                        break;
                }

                if(addr){
                    result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_len)));
                }
            }
        }catch(...){
            SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
            freeifaddrs(results);
            return false;
        }
        freeifaddrs(results);
        return !result.empty();
    }

    bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t> >&result, const std::string& iface, int family){
        //如果网卡没有指定 或者 为任意网卡
        if(iface.empty() || iface == "*"){
            if(family == AF_INET || family == AF_UNSPEC)
                result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
            
            if(family == AF_INET6 || family == AF_UNSPEC)
                result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
            
            return true;
        }

        std::multimap<std::string, std::pair<Address::ptr, uint32_t> > results;

        if(!GetInterfaceAddresses(results, family))
            return false;

        //返回一个迭代器区间 first为首迭代器 second为尾迭代器
        //迭代器包装的内含容器类型仍为multimap<std::string, std::pair<Address::ptr, uint32_t> >
        //equal_range函数用于在序列中表示一个数值的第一次出现与最后一次出现的后一位。得到相等元素的子范围
        auto its = results.equal_range(iface);
        for(; its.first != its.second; ++its.first)
            result.push_back(its.first->second);

        return !result.empty();
    }

    int Address::getFamily() const {
        return getAddr()->sa_family;
    }

    std::string Address::toString() const {
        std::stringstream ss;
        insert(ss);
        return ss.str();
    }

    bool Address::operator<(const Address& rhs) const {
        socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
        int result = memcmp(getAddr(), rhs.getAddr(), minlen);
        if(result < 0)
            return true;
        else if(result > 0)
            return false;
        else if(getAddrLen() < rhs.getAddrLen())
            return true;
        
        return false;
    }

    bool Address::operator==(const Address& rhs) const {
        return getAddrLen() == rhs.getAddrLen()
            && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
    }

    bool Address::operator!=(const Address& rhs) const {
        return !(*this == rhs);
    }



    //block: IPAddress类
    //从文本型网络地址转换为一个实际IP地址。
    //例如，传入一个字符串"192.168.1.1"------>转换为一个真正的通信地址 struct sockaddr_in.sin_addr.s_addr（uint32_t）
    IPAddress::ptr IPAddress::Create(const char* address, uint16_t port){
        addrinfo hints, *results;
        memset(&hints, 0, sizeof(addrinfo));

        hints.ai_flags = AI_NUMERICHOST;
        hints.ai_family = AF_UNSPEC;

        int error = getaddrinfo(address, NULL, &hints, &results);
        if(error){
            SYLAR_LOG_DEBUG(g_logger) << "IPAddress::Create(" << address
                << ", " << port << ") error=" << error
                << " errno=" << errno << " errstr=" << strerror(errno);
            return nullptr;
        }

        try{
            //复用Address类中的Create()函数 由通信结构体创建一个IP地址对象
            IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
                    Address::Create(results->ai_addr,(socklen_t)results->ai_addrlen));
            if(result)
                result->setPort(port);
            
            freeaddrinfo(results);
            return result;
        }catch(...){
            freeaddrinfo(results);
            return nullptr;
        }
    }



    //block: IPv4
    IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port){
        IPv4Address::ptr rt(new IPv4Address);
        rt->m_addr.sin_port = byteswapOnLittleEndian(port);
        int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr); //libfunc:
        /*int inet_pton(int af, const char *src, void *dst);
        convert IPv4 and IPv6 addresses from text to binary form.
        This  function  converts  the character string src into a network address structure in the af address family, 
            then copies the network address structure to dst.  
        The af argument must be either AF_INET or AF_INET6.  dst is written in network byte order.
        */
        if(result <= 0){
            SYLAR_LOG_DEBUG(g_logger) << "IPv4Address::Create(" << address << ", "
                    << port << ") rt=" << result << " errno=" << errno
                    << " errstr=" << strerror(errno);
            return nullptr;
        }
        return rt;
    }

    IPv4Address::IPv4Address(const sockaddr_in& address)
        :m_addr(address){

    }

    IPv4Address::IPv4Address(uint32_t address, uint16_t port){
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = byteswapOnLittleEndian(port);             //转换为大端字节序
        m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);   //转换为大端字节序
    }

    sockaddr* IPv4Address::getAddr(){
        return(sockaddr*)&m_addr;
    }

    const sockaddr* IPv4Address::getAddr() const {
        return(sockaddr*)&m_addr;
    }

    socklen_t IPv4Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream& IPv4Address::insert(std::ostream& os) const {
        uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
        //power:手动转 点分十进制
        os <<((addr >> 24) & 0xff) << "."
           <<((addr >> 16) & 0xff) << "."
           <<((addr >> 8) & 0xff) << "."
           <<(addr & 0xff);
        os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
        return os;
    }

    IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len){
        if(prefix_len > 32)
            return nullptr;

        sockaddr_in baddr(m_addr);
        //power: 广播地址 = 网络地址 | ~子网掩码
        baddr.sin_addr.s_addr |= byteswapOnLittleEndian( CreateMask<uint32_t>(prefix_len) );
        return IPv4Address::ptr(new IPv4Address(baddr));
    }

    IPAddress::ptr IPv4Address::networdAddress(uint32_t prefix_len){
        if(prefix_len > 32)
            return nullptr;

        sockaddr_in baddr(m_addr);
        //power: 广播地址 = 网络地址 & ~子网掩码
        baddr.sin_addr.s_addr &= byteswapOnLittleEndian( CreateMask<uint32_t>(prefix_len) );
        return IPv4Address::ptr(new IPv4Address(baddr));
    }

    IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len){
        sockaddr_in subnet;
        memset(&subnet, 0, sizeof(subnet));
        subnet.sin_family = AF_INET;
        subnet.sin_addr.s_addr = ~byteswapOnLittleEndian( CreateMask<uint32_t>(prefix_len) );   //子网掩码
        return IPv4Address::ptr(new IPv4Address(subnet));
    }

    uint32_t IPv4Address::getPort() const {
        return byteswapOnLittleEndian(m_addr.sin_port);
    }

    void IPv4Address::setPort(uint16_t v){
        m_addr.sin_port = byteswapOnLittleEndian(v);
    }



    //block: IPv6目前不很重要，视频P50 25min P51 12min 以后 
    IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port){
        IPv6Address::ptr rt(new IPv6Address);
        rt->m_addr.sin6_port = byteswapOnLittleEndian(port);
        int result = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
        if(result <= 0){
            SYLAR_LOG_DEBUG(g_logger) << "IPv6Address::Create(" << address << ", "
                    << port << ") rt=" << result << " errno=" << errno
                    << " errstr=" << strerror(errno);
            return nullptr;
        }
        return rt;
    }

    IPv6Address::IPv6Address(){
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
    }

    IPv6Address::IPv6Address(const sockaddr_in6& address){
        m_addr = address;
    }

    IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port){
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
        m_addr.sin6_port = byteswapOnLittleEndian(port);
        memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
    }

    sockaddr* IPv6Address::getAddr(){
        return(sockaddr*)&m_addr;
    }

    const sockaddr* IPv6Address::getAddr() const {
        return(sockaddr*)&m_addr;
    }

    socklen_t IPv6Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream& IPv6Address::insert(std::ostream& os) const {
        os << "[";
        uint16_t* addr =(uint16_t*)m_addr.sin6_addr.s6_addr;
        bool used_zeros = false;
        for(size_t i = 0; i < 8; ++i){
            if(addr[i] == 0 && !used_zeros){
                continue;
            }
            if(i && addr[i - 1] == 0 && !used_zeros){
                os << ":";
                used_zeros = true;
            }
            if(i){
                os << ":";
            }
            os << std::hex <<(int)byteswapOnLittleEndian(addr[i]) << std::dec;
        }

        if(!used_zeros && addr[7] == 0){
            os << "::";
        }

        os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
        return os;
    }

    IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len){
        sockaddr_in6 baddr(m_addr);
        baddr.sin6_addr.s6_addr[prefix_len / 8] |=
            CreateMask<uint8_t>(prefix_len % 8);
        for(int i = prefix_len / 8 + 1; i < 16; ++i){
            baddr.sin6_addr.s6_addr[i] = 0xff;
        }
        return IPv6Address::ptr(new IPv6Address(baddr));
    }

    IPAddress::ptr IPv6Address::networdAddress(uint32_t prefix_len){
        sockaddr_in6 baddr(m_addr);
        baddr.sin6_addr.s6_addr[prefix_len / 8] &=
            CreateMask<uint8_t>(prefix_len % 8);
        for(int i = prefix_len / 8 + 1; i < 16; ++i){
            baddr.sin6_addr.s6_addr[i] = 0x00;
        }
        return IPv6Address::ptr(new IPv6Address(baddr));
    }

    IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len){
        sockaddr_in6 subnet;
        memset(&subnet, 0, sizeof(subnet));
        subnet.sin6_family = AF_INET6;
        subnet.sin6_addr.s6_addr[prefix_len /8] =
            ~CreateMask<uint8_t>(prefix_len % 8);

        for(uint32_t i = 0; i < prefix_len / 8; ++i){
            subnet.sin6_addr.s6_addr[i] = 0xff;
        }
        return IPv6Address::ptr(new IPv6Address(subnet));
    }

    uint32_t IPv6Address::getPort() const {
        return byteswapOnLittleEndian(m_addr.sin6_port);
    }

    void IPv6Address::setPort(uint16_t v){
        m_addr.sin6_port = byteswapOnLittleEndian(v);
    }




    //block: UnixAddress不重要    
    static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

    UnixAddress::UnixAddress(){
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
    }

    UnixAddress::UnixAddress(const std::string& path){
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        m_length = path.size() + 1;

        if(!path.empty() && path[0] == '\0'){
            --m_length;
        }

        if(m_length > sizeof(m_addr.sun_path)){
            throw std::logic_error("path too long");
        }
        memcpy(m_addr.sun_path, path.c_str(), m_length);
        m_length += offsetof(sockaddr_un, sun_path);
    }

    void UnixAddress::setAddrLen(uint32_t v){
        m_length = v;
    }

    sockaddr* UnixAddress::getAddr(){
        return(sockaddr*)&m_addr;
    }

    const sockaddr* UnixAddress::getAddr() const {
        return(sockaddr*)&m_addr;
    }

    socklen_t UnixAddress::getAddrLen() const {
        return m_length;
    }

    std::string UnixAddress::getPath() const {
        std::stringstream ss;
        if(m_length > offsetof(sockaddr_un, sun_path)
                && m_addr.sun_path[0] == '\0'){
            ss << "\\0" << std::string(m_addr.sun_path + 1,
                    m_length - offsetof(sockaddr_un, sun_path) - 1);
        }else {
            ss << m_addr.sun_path;
        }
        return ss.str();
    }

    std::ostream& UnixAddress::insert(std::ostream& os) const {
        if(m_length > offsetof(sockaddr_un, sun_path)
                && m_addr.sun_path[0] == '\0'){
            return os << "\\0" << std::string(m_addr.sun_path + 1,
                    m_length - offsetof(sockaddr_un, sun_path) - 1);
        }
        return os << m_addr.sun_path;
    }




    //block: UnknownAddress
    UnknownAddress::UnknownAddress(int family){
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sa_family = family;
    }

    UnknownAddress::UnknownAddress(const sockaddr& addr){
        m_addr = addr;
    }

    sockaddr* UnknownAddress::getAddr(){
        return(sockaddr*)&m_addr;
    }

    const sockaddr* UnknownAddress::getAddr() const {
        return &m_addr;
    }

    socklen_t UnknownAddress::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream& UnknownAddress::insert(std::ostream& os) const {
        os << "[UnknownAddress family=" << m_addr.sa_family << "]";
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const Address& addr){
        return addr.insert(os);
    }

}

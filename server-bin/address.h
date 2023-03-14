//网络地址的封装(IPv4,IPv6,Unix)
#ifndef __SYLAR_ADDRESS_H__
#define __SYLAR_ADDRESS_H__

#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <map>

namespace sylar {
    /*
    //skip: Ipv6、UnixSocket     
    */

    class IPAddress;

    //网络地址的基类,抽象类
    class Address {
    public:
        typedef std::shared_ptr<Address> ptr;

        /**
         * @brief 根据传入的sockaddr指针中的协议类型创建对应的通信地址子类对象，返回和sockaddr相匹配的Address,失败返回nullptr
         * @param[in] addr sockaddr指针
         * @param[in] addrlen sockaddr的长度*/
        static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);

        /**
         * @param[in] host 域名,服务器名等.举例: www.sylar.top[:80] (方括号为可选内容)
         * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
         * @param[in] type socketl类型SOCK_STREAM、SOCK_DGRAM 等
         * @param[in] protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等  
         * @param[out] result 保存满足条件的Address       
        **/
        //通过host地址返回对应条件的所有Address，返回是否转换成功，失败返回UnknownAddress
        static bool Lookup(std::vector<Address::ptr>& result, const std::string& host, int family = AF_INET, int type = 0, int protocol = 0);
        //通过host地址返回对应条件的任意Address，返回满足条件的任意Address,失败返回nullptr
        static Address::ptr LookupAny(const std::string& host, int family = AF_INET, int type = 0, int protocol = 0);
        //通过host地址返回对应条件的任意IPAddress，返回满足条件的任意IPAddress,失败返回nullptr
        static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host, int family = AF_INET, int type = 0, int protocol = 0);
        
        //返回本机所有网卡的<网卡名, 地址, 子网掩码位数>，保存到result中，返回是否获取成功
        static bool GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t> >& result, int family = AF_INET);
        //获取指定网卡的地址和子网掩码位数，没有指定或指定为任意网卡名称，就返回一个默认的IP地址，返回是否获取成功 iface：网卡名称
        static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t> >&result, const std::string& iface, int family = AF_INET);

        virtual ~Address(){}

        int getFamily() const; //返回协议簇

        virtual const sockaddr* getAddr() const = 0;    //返回sockaddr指针,只读
        virtual sockaddr* getAddr() = 0;                //返回sockaddr指针,读写
        virtual socklen_t getAddrLen() const = 0;       //获取通信结构体长度，返回sockaddr的长度

        virtual std::ostream& insert(std::ostream& os) const = 0;   //可读性输出地址
        std::string toString() const;                               //返回可读性字符串

        bool operator<(const Address& rhs) const;   //小于号比较函数
        bool operator==(const Address& rhs) const;  //等于函数
        bool operator!=(const Address& rhs) const;  //不等于函数
    };



    //IP地址的基类
    class IPAddress : public Address {
    public:
        typedef std::shared_ptr<IPAddress> ptr;

        //通过域名,IP,服务器名创建IPAddress，调用成功返回IPAddress,失败返回nullptr； address 域名,IP,服务器名等.举例: www.sylar.top， port 端口号
        static IPAddress::ptr Create(const char* address, uint16_t port = 0);

        virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;   //获取该地址的广播地址，成功返回IPAddress,失败返回nullptr； prefix_len：子网掩码位数
        virtual IPAddress::ptr networdAddress(uint32_t prefix_len) = 0;     //获取该地址的网段，成功返回IPAddress,失败返回nullptr； prefix_len：子网掩码位数
        virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;         //获取子网掩码地址，成功返回IPAddress,失败返回nullptr； prefix_len：子网掩码位数
        virtual uint32_t getPort() const = 0;   //返回端口号
        virtual void setPort(uint16_t v) = 0;   //设置端口号
    };




    //IPv4地址
    class IPv4Address : public IPAddress {
    public:
        typedef std::shared_ptr<IPv4Address> ptr;

        //创建IPv4Address,成功返回IPv4Address,失败返回nullptr; address：点分十进制地址,如:192.168.1.1, port：端口号
        static IPv4Address::ptr Create(const char* address, uint16_t port = 0);

        IPv4Address(const sockaddr_in& address);    //通过sockaddr_in构造IPv4Address， address：sockaddr_in结构体
        IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);  //通过二进制地址构造IPv4Address  address：二进制地址address，port：端口号

        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;  //hack: 这里为什么要const呢

        IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
        IPAddress::ptr networdAddress(uint32_t prefix_len) override;
        IPAddress::ptr subnetMask(uint32_t prefix_len) override;
        uint32_t getPort() const override;
        void setPort(uint16_t v) override;

    private:
        sockaddr_in m_addr;
    };

    //IPv6地址
    class IPv6Address : public IPAddress {
    public:
        typedef std::shared_ptr<IPv6Address> ptr;

        //通过IPv6地址字符串构造IPv6Address, address: IPv6地址字符串 port: 端口号
        static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

        IPv6Address();
        IPv6Address(const sockaddr_in6& address);   //通过sockaddr_in6构造IPv6Address, address: sockaddr_in6结构体
        IPv6Address(const uint8_t address[16], uint16_t port = 0);  //通过IPv6二进制地址构造IPv6Address, address: IPv6二进制地址

        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;

        IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
        IPAddress::ptr networdAddress(uint32_t prefix_len) override;
        IPAddress::ptr subnetMask(uint32_t prefix_len) override;
        uint32_t getPort() const override;
        void setPort(uint16_t v) override;

    private:
        sockaddr_in6 m_addr;
    };

    //UnixSocket地址
    class UnixAddress : public Address {
    public:
        typedef std::shared_ptr<UnixAddress> ptr;

        UnixAddress();
        UnixAddress(const std::string& path);   //通过路径构造UnixAddress path: UnixSocket路径(长度小于UNIX_PATH_MAX)

        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        void setAddrLen(uint32_t v);
        std::string getPath() const;
        std::ostream& insert(std::ostream& os) const override;

    private:
        sockaddr_un m_addr;
        socklen_t m_length;
    };

    //未知地址
    class UnknownAddress : public Address {
    public:
        typedef std::shared_ptr<UnknownAddress> ptr;
        UnknownAddress(int family);
        UnknownAddress(const sockaddr& addr);
        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;

    private:
        sockaddr m_addr;//TODO: 改成m_sockaddr
    };




    //流式输出Address
    std::ostream& operator<<(std::ostream& os, const Address& addr);

}

#endif

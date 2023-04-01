#include "socket_stream.h"
#include "IOCoroutineScheduler/util.h"

namespace bin {

    SocketStream::SocketStream(Socket::ptr sock, bool owner)
        :m_socket(sock)
        ,m_owner(owner){
    }

    SocketStream::~SocketStream(){
        if(m_owner && m_socket){
            m_socket->close();
        }
    }

    bool SocketStream::isConnected() const {
        return m_socket && m_socket->isConnected();
    }

    int SocketStream::read(void* buffer, size_t length){
        if(!isConnected()){
            return -1;
        }
        return m_socket->recv(buffer, length);
    }

    int SocketStream::read(ByteArray::ptr ba, size_t length){
        if(!isConnected())
            return -1;
        
        //通过iovec和ByteArray内存空间建立映射
        //获取到即将写入的空间
        std::vector<iovec> iovs;
        ba->getWriteBuffers(iovs, length);

        //接收数据 直接就写入到了指向ByteArray的内存空间中
        int rt = m_socket->recv(&iovs[0], iovs.size());
        if(rt > 0){ //由于getWriteBuf() 并不会修改内存指针位置 手动修改m_position
            ba->setPosition(ba->getPosition() + rt);
        }
        return rt;
    }

    int SocketStream::write(const void* buffer, size_t length){
        if(!isConnected()){
            return -1;
        }
        return m_socket->send(buffer, length);
    }

    int SocketStream::write(ByteArray::ptr ba, size_t length){
        if(!isConnected()){
            return -1;
        }
        std::vector<iovec> iovs;
        ba->getReadBuffers(iovs, length);
        int rt = m_socket->send(&iovs[0], iovs.size());
        if(rt > 0){
            ba->setPosition(ba->getPosition() + rt);
        }
        return rt;
    }

    void SocketStream::close(){
        if(m_socket){
            m_socket->close();
        }
    }

    Address::ptr SocketStream::getRemoteAddress(){
        if(m_socket){
            return m_socket->getRemoteAddress();
        }
        return nullptr;
    }

    Address::ptr SocketStream::getLocalAddress(){
        if(m_socket){
            return m_socket->getLocalAddress();
        }
        return nullptr;
    }

    std::string SocketStream::getRemoteAddressString(){
        auto addr = getRemoteAddress();
        if(addr){
            return addr->toString();
        }
        return "";
    }

    std::string SocketStream::getLocalAddressString(){
        auto addr = getLocalAddress();
        if(addr){
            return addr->toString();
        }
        return "";
    }

}

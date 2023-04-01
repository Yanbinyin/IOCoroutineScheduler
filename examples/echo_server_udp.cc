#include "IOCoroutineScheduler/socket.h"
#include "IOCoroutineScheduler/log.h"
#include "IOCoroutineScheduler/iomanager.h"

static bin::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run(){
    bin::IPAddress::ptr addr = bin::Address::LookupAnyIPAddress("0.0.0.0:8050");
    bin::Socket::ptr sock = bin::Socket::CreateUDP(addr);
    if(sock->bind(addr)){
        SYLAR_LOG_INFO(g_logger) << "udp bind : " << *addr;
    } else {
        SYLAR_LOG_ERROR(g_logger) << "udp bind : " << *addr << " fail";
        return;
    }
    while(true){
        char buff[1024];
        bin::Address::ptr from(new bin::IPv4Address);
        int len = sock->recvFrom(buff, 1024, from);
        if(len > 0){
            buff[len] = '\0';
            SYLAR_LOG_INFO(g_logger) << "recv: " << buff << " from: " << *from;
            len = sock->sendTo(buff, len, from);
            if(len < 0){
                SYLAR_LOG_INFO(g_logger) << "send: " << buff << " to: " << *from
                    << " error=" << len;
            }
        }
    }
}

int main(int argc, char** argv){
    bin::IOManager iom(1);
    iom.schedule(run);
    return 0;
}

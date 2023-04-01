#include "IOCoroutineScheduler/socket.h"
#include "IOCoroutineScheduler/iomanager.h"
#include "IOCoroutineScheduler/log.h"
#include <stdlib.h>

static bin::Logger::ptr g_logger = BIN_LOG_ROOT();

const char* ip = nullptr;
uint16_t port = 0;

void run(){
    bin::IPAddress::ptr addr = bin::Address::LookupAnyIPAddress(ip);
    if(!addr){
        BIN_LOG_ERROR(g_logger) << "invalid ip: " << ip;
        return;
    }
    addr->setPort(port);

    bin::Socket::ptr sock = bin::Socket::CreateUDP(addr);

    bin::IOManager::GetThis()->schedule([sock](){
            bin::Address::ptr addr(new bin::IPv4Address);
            BIN_LOG_INFO(g_logger) << "begin recv";
            while(true){
                char buff[1024];
                int len = sock->recvFrom(buff, 1024, addr);
                if(len > 0){
                    std::cout << std::endl << "recv: " << std::string(buff, len) << " from: " << *addr << std::endl;
                }
            }
    });
    sleep(1);
    while(true){
        std::string line;
        std::cout << "input>";
        std::getline(std::cin, line);
        if(!line.empty()){
            int len = sock->sendTo(line.c_str(), line.size(), addr);
            if(len < 0){
                int err = sock->getError();
                BIN_LOG_ERROR(g_logger) << "send error err=" << err
                        << " errstr=" << strerror(err) << " len=" << len
                        << " addr=" << *addr
                        << " sock=" << *sock;
            } else {
                BIN_LOG_INFO(g_logger) << "send " << line << " len:" << len;
            }
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        BIN_LOG_INFO(g_logger) << "use as[" << argv[0] << " ip port]";
        return 0;
    }
    ip = argv[1];
    port = atoi(argv[2]);
    bin::IOManager iom(2);
    iom.schedule(run);
    return 0;
}

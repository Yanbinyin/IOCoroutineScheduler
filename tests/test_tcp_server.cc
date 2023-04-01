#include "IOCoroutineScheduler/tcp_server.h"
#include "IOCoroutineScheduler/iomanager.h"
#include "IOCoroutineScheduler/log.h"

bin::Logger::ptr g_logger = BIN_LOG_ROOT();
//test_tcp_server.cc 还有就是example里面的 echo_server
void run(){
    auto addr = bin::Address::LookupAny("0.0.0.0:8033");
    auto addr2 = bin::UnixAddress::ptr(new bin::UnixAddress("/tmp/unix_addr"));
    std::vector<bin::Address::ptr> addrs;
    addrs.push_back(addr);
    addrs.push_back(addr2);

    bin::TcpServer::ptr tcp_server(new bin::TcpServer);
    std::vector<bin::Address::ptr> fails;
    while(!tcp_server->bind(addrs, fails)){
        sleep(2);
    }
    tcp_server->start();
    
}


int main(int argc, char** argv){
    bin::IOManager iom(2);
    iom.schedule(run);
    return 0;
}
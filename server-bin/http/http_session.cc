#include "http_session.h"
#include "http_parser.h"

namespace sylar::http {
//namespace http {
    
    HttpSession::HttpSession(Socket::ptr sock, bool owner)
        :SocketStream(sock, owner){
    }

    //收到HTTP请求报文
    HttpRequest::ptr HttpSession::recvRequest(){
        //核心逻辑：先解析出报文头部，再解析报文实体

        //1. 创建HttpRequestParser对象，获取当前所能接收的最大报文首部长度，根据这个长度创建一块缓冲区用于暂存接收到的请求报文
        HttpRequestParser::ptr parser(new HttpRequestParser);

        //使用某一时刻的值即可 不需要实时的值
        uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
        //uint64_t buff_size = 100; //bin:数值太小，请求标头就会接收不全

         //创建一个接受请求报文的缓冲区 指定析构
        std::shared_ptr<char> buffer(
                new char[buff_size], [](char* ptr){
                    delete[] ptr;
                });
        char* data = buffer.get();
        int offset = 0;

        /*2. 采取一边接收报文，一边解析报文头部的策略：
        while(1){
            a. 尝试通过套接字Socket对象接收报文
            b. 根据实际接收到的报文长度ret，尝试调用HttpRequestParser::execute()执行报文解析动作，并且计算
                一个偏移量offset = 实际接收到的报文长度 - 实际解析的报文长度判断当前接收的却没有解析掉的报文是否超过最大限定值（4KB）。
            c. 判断解析是否已经全部完成：是，就执行break；否则，继续接收报文，继续解析。
        }*/
        do {
            int len = read(data + offset, buff_size - offset); //len表示缓冲区可读数据的大小
            if(len <= 0){
                close();
                return nullptr;
            }
            len += offset;
            size_t nparse = parser->execute(data, len);
            if(parser->hasError()){
                close();
                return nullptr;
            }
            offset = len - nparse;
            if(offset == (int)buff_size){
                close();
                return nullptr;
            }
            //如果解析已经结束
            if(parser->isFinished()){
                break;
            }
        } while(true);

        /*3. 存储报文实体内容。获取报文实体的长度大小，根据报文实体长度 和 2. 中得到的一个偏移量offset来判断一下当前的报文实体是否接收完整：
            a. 报文实体长度 > offset， 说明当前收到的报文实体不完整。设置一个循环，再一次接收客户端传输来的报文实体，直至报文实体完整。
            b. 报文实体长度 <=  offset，说明当前收到的报文实体已经完整*/
        int64_t length = parser->getContentLength(); //获取实体长度
        //将报文实体读出 并且设置到HttpRequest对象中去
        if(length > 0){
            std::string body;
            body.resize(length);

            int len = 0;
            //这里offset 就是报文首部解析完之后 剩余的报文实体的字节数
            if(length >= offset){
                memcpy(&body[0], data, offset);
                len = offset;
            }else{
                memcpy(&body[0], data, length);
                len = length;
            }
            length -= offset;
            //如果还有剩余的报文实体长度 说明报文实体没有在本次传输中全部被接收
            //设置一个循环继续接收报文实体
            if(length > 0){
                if(readFixSize(&body[len], length) <= 0){
                    close();
                    return nullptr;
                }
            }

            //4. 将完整的报文实体放入HttpRequestParser::HttpRequest对象之中
            parser->getData()->setBody(body);
        }

        parser->getData()->init();
        return parser->getData();
    }

    int HttpSession::sendResponse(HttpResponse::ptr rsp){
        std::stringstream ss;
        ss << *rsp;
        std::string data = ss.str();
        return writeFixSize(data.c_str(), data.size());
    }

//}
}

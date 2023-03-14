//流接口: 

#ifndef __SYLAR_STREAM_H__
#define __SYLAR_STREAM_H__

#include <memory>
#include "bytearray.h"

namespace sylar {

    //流结构:对文件操作/Socket 的读写操作进行一层Stream流式封装，屏蔽他们之间的一些差异，使得它们的操作更加便捷化
    /*
     *      @retval >0 返回接收到的数据的实际大小
     *      @retval =0 被关闭
     *      @retval <0 出现流错误
     */
    class Stream {
    public:
        typedef std::shared_ptr<Stream> ptr;

        virtual ~Stream(){}

        virtual int read(void* buffer, size_t length) = 0;              //读长度为length的数据到buffer中
        virtual int read(ByteArray::ptr ba, size_t length) = 0;         //读长度为length的数据到ba(ByteArray)中
        virtual int readFixSize(void* buffer, size_t length);           //读固定长度为length的数据到buffer中
        virtual int readFixSize(ByteArray::ptr ba, size_t length);      //读固定长度为length的数据ba(ByteArray)中
        virtual int write(const void* buffer, size_t length) = 0;       //写长度为length的数据到buffer中
        virtual int write(ByteArray::ptr ba, size_t length) = 0;        //写长度为length的数据到ba(ByteArray)中
        virtual int writeFixSize(const void* buffer, size_t length);    //写固定长度为length的数据到buffer中
        virtual int writeFixSize(ByteArray::ptr ba, size_t length);     //写固定长度为length的数据ba(ByteArray)中
        virtual void close() = 0;                                       //关闭流
    };

}

#endif

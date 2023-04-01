#include "IOCoroutineScheduler/bytearray.h"
#include "IOCoroutineScheduler/bin.h"

static bin::Logger::ptr g_logger = BIN_LOG_ROOT();

void test(){
#define XX(type, len, write_fun, read_fun, base_len){\
    std::vector<type> vec; \
    /*1.生成一个随机数组，创建Bytearray对象（开辟一块动态内存空间）*/ \
    for(int i = 0; i < len; ++i) \
        vec.push_back(rand()); /*rand()产生[0, 99]的整数*/\
    bin::ByteArray::ptr ba(new bin::ByteArray(base_len)); \
    /*2.依次将数组内容写入内存空间*/\
    for(auto& i : vec) \
        ba->write_fun(i); \
    \
    /*3.手动将内存指针置为开头ba->setPosition(0)，开始读取*/\
    ba->setPosition(0); \
    for(size_t i = 0; i < vec.size(); ++i){ \
        type v = ba->read_fun(); \
        BIN_ASSERT(v == vec[i]); \
    } \
    BIN_ASSERT(ba->getReadSize() == 0); \
    /*4.显示一下内存空间开辟的总容量，已经实际使用的空间大小，内存结点个数等信息*/\
    BIN_LOG_INFO(g_logger) << #write_fun "\t/" #read_fun " \t(" #type " )\t len=" << len \
                    << " base_len=" << base_len << " size=" << ba->getSize(); \
}

    /*固定大小*/
    // XX(int8_t,  100, writeFint8, readFint8, 1);
    // XX(uint8_t, 100, writeFuint8, readFuint8, 1);
    // XX(int16_t,  100, writeFint16,  readFint16, 1);
    // XX(uint16_t, 100, writeFuint16, readFuint16, 1);
    XX(int32_t,  100, writeFint32,  readFint32, 1);
    XX(uint32_t, 100, writeFuint32, readFuint32, 1);
    XX(int64_t,  100, writeFint64,  readFint64, 1);
    XX(uint64_t, 100, writeFuint64, readFuint64, 1);

    /*变长大小*/
    XX(int32_t,  100, writeInt32,  readInt32, 1);
    XX(uint32_t, 100, writeUint32, readUint32, 1);
    XX(int64_t,  100, writeInt64,  readInt64, 1);
    XX(uint64_t, 100, writeUint64, readUint64, 1);
#undef XX

#define XX(type, len, write_fun, read_fun, base_len){\
    std::vector<type> vec; \
    for(int i = 0; i < len; ++i){ \
        vec.push_back(rand()); \
    } \
    bin::ByteArray::ptr ba(new bin::ByteArray(base_len)); \
    for(auto& i : vec){ \
        ba->write_fun(i); \
    } \
    ba->setPosition(0); \
    for(size_t i = 0; i < vec.size(); ++i){ \
        type v = ba->read_fun(); \
        BIN_ASSERT(v == vec[i]); \
    } \
    BIN_ASSERT(ba->getReadSize() == 0); \
    BIN_LOG_INFO(g_logger) << #write_fun "/" #read_fun " (" #type " ) len=" << len \
                    << " base_len=" << base_len << " size=" << ba->getSize(); \
    /*想对于上一个的不同体现在下面这些方面*/ \
    ba->setPosition(0); \
    BIN_ASSERT(ba->writeToFile("/tmp/" #type "_" #len "-" #read_fun ".dat")); \
    bin::ByteArray::ptr ba2(new bin::ByteArray(base_len * 2)); \
    BIN_ASSERT(ba2->readFromFile("/tmp/" #type "_" #len "-" #read_fun ".dat")); \
    ba2->setPosition(0); \
    BIN_ASSERT(ba->toString() == ba2->toString()); \
    BIN_ASSERT(ba->getPosition() == 0); \
    BIN_ASSERT(ba2->getPosition() == 0); \
}
    XX(int8_t,  100, writeFint8, readFint8, 1);
    XX(uint8_t, 100, writeFuint8, readFuint8, 1);
    XX(int16_t,  100, writeFint16,  readFint16, 1);
    XX(uint16_t, 100, writeFuint16, readFuint16, 1);
    XX(int32_t,  100, writeFint32,  readFint32, 1);
    XX(uint32_t, 100, writeFuint32, readFuint32, 1);
    XX(int64_t,  100, writeFint64,  readFint64, 1);
    XX(uint64_t, 100, writeFuint64, readFuint64, 1);

    XX(int32_t,  100, writeInt32,  readInt32, 1);
    XX(uint32_t, 100, writeUint32, readUint32, 1);
    XX(int64_t,  100, writeInt64,  readInt64, 1);
    XX(uint64_t, 100, writeUint64, readUint64, 1);

#undef XX


}

int main(int argc, char** argv){
    test();
    return 0;
}

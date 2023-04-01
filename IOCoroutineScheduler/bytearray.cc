#include "bytearray.h"
#include <fstream>
#include <sstream>
#include <string.h>
#include <iomanip>

#include "endian.h"
#include "log.h"

namespace bin{

    static bin::Logger::ptr g_logger = BIN_LOG_NAME("system");


    ByteArray::Node::Node(size_t s)
        :ptr(new char[s])
        ,next(nullptr)
        ,size(s){
    }

    ByteArray::Node::Node()
        :ptr(nullptr)
        ,next(nullptr)
        ,size(0){
    }

    ByteArray::Node::~Node(){
        if(ptr)
            delete[] ptr;
    }



    //构造析构函数
    ByteArray::ByteArray(size_t base_size)
        :m_baseSize(base_size)
        ,m_position(0)
        ,m_capacity(base_size)
        ,m_size(0)
        ,m_endian(BIN_BIG_ENDIAN)
        ,m_root(new Node(base_size))
        ,m_cur(m_root){
    }

    ByteArray::~ByteArray(){
        Node* tmp = m_root;
        while(tmp){
            m_cur = tmp;
            tmp = tmp->next;
            delete m_cur;
        }
    }



    /*//block:辅助函数：
        varint算法对负数压缩的效率很低下，符号位是1。将有符号的数都转为无符号数进行varint压缩。
        整数的转换值为原来的两倍，负数的转换值为原来的两倍再+1，转换后+x和-x是大小相邻。
        存入时候负---->正，取出时候恢复正----->负
    */
   
    //int32_t---------->uint32_t
    static uint32_t EncodeZigzag32(const int32_t& v){
        //不转换的话 负数压缩会浪费空间
        if(v < 0){
            return ((uint32_t)(-v)) * 2 - 1;
        }else{
            return v * 2;
        }
    }

    //int64_t---------->uint64_t 
    static uint64_t EncodeZigzag64(const int64_t& v){
        //不转换的话 -1压缩一定要消耗10个字节
        if(v < 0){
            return ((uint64_t)(-v)) * 2 - 1;
        }else{
            return v * 2;
        }
    }

    //uint32_t---------->int32_t
    static int32_t DecodeZigzag32(const uint32_t& v){
        //消除乘2  异或一下最后一位来恢复负数
        return (v >> 1) ^ -(v & 1); // v>>1 除2 | v&1 通过判断v是偶数还是奇数来判断zigzag之前的正负
    }

    //uint64_t---------->int64_t
    static int64_t DecodeZigzag64(const uint64_t& v){
        //消除乘2  异或一下最后一位来恢复负数
        return (v >> 1) ^ -(v & 1);
    }



    //操作大小端
    bool ByteArray::isLittleEndian() const{
        return m_endian == BIN_LITTLE_ENDIAN;
    }

    void ByteArray::setIsLittleEndian(bool val){
        if(val){
            m_endian = BIN_LITTLE_ENDIAN;
        }else{
            m_endian = BIN_BIG_ENDIAN;
        }
    }



    //block:写入固定长度数据
    void ByteArray::writeFint8  (int8_t value){
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint8 (uint8_t value){
        write(&value, sizeof(value));
    }

    void ByteArray::writeFint16 (int16_t value){
        //当前字节序和本机字节序不符 需要swap
        if(m_endian != BIN_BYTE_ORDER)
            value = byteswap(value);
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint16(uint16_t value){
        //当前字节序和本机字节序不符 需要swap
        if(m_endian != BIN_BYTE_ORDER){
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    void ByteArray::writeFint32 (int32_t value){
        //当前字节序和本机字节序不符 需要swap
        if(m_endian != BIN_BYTE_ORDER)
            value = byteswap(value);
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint32(uint32_t value){
        //当前字节序和本机字节序不符 需要swap
        if(m_endian != BIN_BYTE_ORDER)
            value = byteswap(value);
        write(&value, sizeof(value));
    }

    void ByteArray::writeFint64 (int64_t value){
        //当前字节序和本机字节序不符 需要swap
        if(m_endian != BIN_BYTE_ORDER)
            value = byteswap(value);
        write(&value, sizeof(value));
    }

    void ByteArray::writeFuint64(uint64_t value){
        //当前字节序和本机字节序不符 需要swap
        if(m_endian != BIN_BYTE_ORDER)
            value = byteswap(value);
        write(&value, sizeof(value));
    }


    //block：varint编码
    void ByteArray::writeInt32  (int32_t value){
        writeUint32(EncodeZigzag32(value));
    }

    void ByteArray::writeUint32 (uint32_t value){
        //uint32_t 压缩后1~5字节的大小
        uint8_t tmp[5];
        uint8_t i = 0;
        //varint编码 msp等于1 就认为数据还没读完
        while(value >= 0x80){   //0x80 1000 0000
            //取低7位 + msp==1 组成新的编码数据
            tmp[i++] = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        tmp[i++] = value;
        write(tmp, i);
    }

    void ByteArray::writeInt64  (int64_t value){
        writeUint64(EncodeZigzag64(value));
    }

    void ByteArray::writeUint64 (uint64_t value){
        //uint64_t 压缩后1~10字节的大小
        uint8_t tmp[10];
        uint8_t i = 0;
        //varint编码 msp等于1 就认为数据还没读完
        while(value >= 0x80){
            //取低7位+msp==1 组成新的编码数据
            tmp[i++] = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        tmp[i++] = value;
        write(tmp, i);
    }

    void ByteArray::writeFloat  (float value){
        uint32_t v;
        memcpy(&v, &value, sizeof(value));
        writeFuint32(v);
    }

    void ByteArray::writeDouble (double value){
        uint64_t v;
        memcpy(&v, &value, sizeof(value));
        writeFuint64(v);
    }

    void ByteArray::writeStringF16(const std::string& value){
        writeFuint16(value.size());
        write(value.c_str(), value.size());
    }

    void ByteArray::writeStringF32(const std::string& value){
        writeFuint32(value.size());
        write(value.c_str(), value.size());
    }

    void ByteArray::writeStringF64(const std::string& value){
        writeFuint64(value.size());
        write(value.c_str(), value.size());
    }

    void ByteArray::writeStringVint(const std::string& value){
        writeUint64(value.size());
        write(value.c_str(), value.size());
    }

    void ByteArray::writeStringWithoutLength(const std::string& value){
        write(value.c_str(), value.size());
    }


    //block：读取固定长度数据
    int8_t   ByteArray::readFint8(){
        int8_t v;
        read(&v, sizeof(v));
        return v;
    }

    uint8_t  ByteArray::readFuint8(){
        uint8_t v;
        read(&v, sizeof(v));
        return v;
    }

    #define XX(type) \
        type v; \
        read(&v, sizeof(v)); \
        if(m_endian == BIN_BYTE_ORDER){ /*检测字节序是大端还是小端*/\
            return v; \
        }else{ \
            return byteswap(v); \
        }

    int16_t  ByteArray::readFint16(){
        XX(int16_t);
    }

    uint16_t ByteArray::readFuint16(){
        XX(uint16_t);
    }

    int32_t  ByteArray::readFint32(){
        XX(int32_t);
    }

    uint32_t ByteArray::readFuint32(){
        XX(uint32_t);
    }

    int64_t  ByteArray::readFint64(){
        XX(int64_t);
    }

    uint64_t ByteArray::readFuint64(){
        XX(uint64_t);
    }

    #undef XX
    
    
    //block：写入varint压缩编码数据
    int32_t  ByteArray::readInt32(){
        return DecodeZigzag32(readUint32());
    }

    uint32_t ByteArray::readUint32(){
        //最终得到一个uint32_t型数据
        uint32_t result = 0;
        //max读取次数 = 32 / 7 + 1 组
        for(int i = 0; i < 32; i += 7){
            uint8_t b = readFuint8();   //一次读取8位
            if(b < 0x80){   //msp==0 说明这是该数据的最后一个字节
                result |= ((uint32_t)b) << i;
                break;
            }else{  //msp==1 说明后面还有字节没有取 要去掉头部的msp位
                result |= (((uint32_t)(b & 0x7f)) << i);
            }
        }
        return result;
    }

    int64_t  ByteArray::readInt64(){
        return DecodeZigzag64(readUint64());
    }

    uint64_t ByteArray::readUint64(){
        //最终得到一个uint32_t型数据
        uint64_t result = 0;
        //max读取次数 = 64 / 7 + 1 组
        for(int i = 0; i < 64; i += 7){
            uint8_t b = readFuint8();
            if(b < 0x80){   //msp==0 说明这是该数据的最后一个字节
                result |= ((uint64_t)b) << i;   //最后一个字节直接或运算 不用去msp位
                break;
            }else{  //msp==1 说明后面还有字节没有取 要去掉头部的msp位
                result |= (((uint64_t)(b & 0x7f)) << i);
            }
        }
        return result;
    }

    float    ByteArray::readFloat(){
        uint32_t v = readFuint32();
        float value;
        memcpy(&value, &v, sizeof(v));
        return value;
    }

    double   ByteArray::readDouble(){
        uint64_t v = readFuint64();
        double value;
        memcpy(&value, &v, sizeof(v));
        return value;
    }

    std::string ByteArray::readStringF16(){
        uint16_t len = readFuint16();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    std::string ByteArray::readStringF32(){
        uint32_t len = readFuint32();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    std::string ByteArray::readStringF64(){
        uint64_t len = readFuint64();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    std::string ByteArray::readStringVint(){
        uint64_t len = readUint64();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }



    void ByteArray::clear(){
        m_position = m_size = 0;
        m_capacity = m_baseSize;
        Node* tmp = m_root->next;
        while(tmp){
            m_cur = tmp;
            tmp = tmp->next;
            delete m_cur;
        }
        m_cur = m_root;
        m_root->next = NULL;
    }

    void ByteArray::write(const void* buf, size_t size){
        if(size == 0)
            return;
        
        addCapacity(size);  //保险起见，先addCapacity size个字节

        size_t npos = m_position % m_baseSize;  //内存指针现在在内存块结点哪一个字节位置上
        size_t ncap = m_cur->size - npos;   //当前结点的剩余容量
        size_t bpos = 0;    //已经写入内存的数据量

        while(size > 0){
            if(ncap >= size){   //内存结点当前剩余容量能放下size的数据
                memcpy(m_cur->ptr + npos, (const char*)buf + bpos, size);
                if(m_cur->size == (npos + size))    //正好把这一块填满
                    m_cur = m_cur->next;
                m_position += size;
                bpos += size;
                size = 0;
            }else{  //不够放 先把当前剩余空间写完 在下一个新结点继续写入
                memcpy(m_cur->ptr + npos, (const char*)buf + bpos, ncap);   //复制ncap个bytes从buf+bpos到m_cur->ptr+npos
                m_position += ncap;
                bpos += ncap;
                size -= ncap;
                //去遍历下一个内存块
                m_cur = m_cur->next;
                ncap = m_cur->size;
                npos = 0;
            }
        }

        //如果内存指针超过了当前表示的已经使用的空间大小 更新一下
        if(m_position > m_size)
            m_size = m_position; 
    }

    //将内存块的内容读取到缓冲区中。和write()一模一样的流程，读什么位置依赖于内存指针的位置，始终从内存指针m_position一直读到最后
    void ByteArray::read(void* buf, size_t size){
        if(size > getReadSize()) //读取的长度超出可读范围要抛异常
            throw std::out_of_range("not enough len");

        size_t npos = m_position % m_baseSize; //内存指针现在在内存块结点哪一个字节位置上
        size_t ncap = m_cur->size - npos; //当前结点剩余容量
        size_t bpos = 0; //当前已经读取的数据量
        while(size > 0){
            if(ncap >= size){
                memcpy((char*)buf + bpos, m_cur->ptr + npos, size);
                if(m_cur->size == (npos + size))    //如果当前结点被读完
                    m_cur = m_cur->next;
                m_position += size;
                bpos += size;
                size = 0;
            }else{
                memcpy((char*)buf + bpos, m_cur->ptr + npos, ncap);
                m_position += ncap;
                bpos += ncap;
                size -= ncap;
                m_cur = m_cur->next;
                ncap = m_cur->size;
                npos = 0;
            }
        }
    }

    //将内存块的内容读取到缓冲区中，但不影响当前内存指针指向的位置，使用一个外部传入的内存指针position，而不使用当前真正的内存指针m_position。
    //即：用户只关心存储的内容，而不关心是否移除内存中的内容，或许还要紧接着写入内容
    void ByteArray::read(void* buf, size_t size, size_t position) const{
        if(size > (m_size - position))
            throw std::out_of_range("not enough len");

        size_t npos = position % m_baseSize;
        size_t ncap = m_cur->size - npos;
        size_t bpos = 0;
        Node* cur = m_cur;
        while(size > 0){
            if(ncap >= size){
                memcpy((char*)buf + bpos, cur->ptr + npos, size);
                if(cur->size == (npos + size))
                    cur = cur->next;
                position += size;
                bpos += size;
                size = 0;
            }else{
                memcpy((char*)buf + bpos, cur->ptr + npos, ncap);
                position += ncap;
                bpos += ncap;
                size -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
        }
    }

    void ByteArray::setPosition(size_t val){
        //设置内存指针位置超出了当前总容量大小 抛出异常
        if(val > m_capacity)
            throw std::out_of_range("set_position out of range");
        
        m_position = val;
        if(m_position > m_size)
            m_size = m_position; //检查内存指针位置 > 使用内存空间大小要进行更新 不更新这里，echo_server.cc就无法输出，telnet连接发送的内容
        
        m_cur = m_root;

        /*移动当前可用结点指针m_cur*/
        //只要设定值val比单个结点容量大小大 认为没达到预期设定位置
        while(val > m_cur->size){
            val -= m_cur->size;
            m_cur = m_cur->next;
        }
        if(val == m_cur->size){
            m_cur = m_cur->next;
        }
    }

    bool ByteArray::writeToFile(const std::string& name) const{
        std::ofstream ofs;
        ofs.open(name, std::ios::trunc | std::ios::binary);
        if(!ofs){
            BIN_LOG_ERROR(g_logger) << "writeToFile name=" << name
                << " error , errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }

        int64_t read_size = getReadSize();
        int64_t pos = m_position;
        Node* cur = m_cur;

        while(read_size > 0){
            int diff = pos % m_baseSize;
            int64_t len = (read_size > (int64_t)m_baseSize ? m_baseSize : read_size) - diff;
            ofs.write(cur->ptr + diff, len);
            cur = cur->next;
            pos += len;
            read_size -= len;
        }

        return true;
    }

    bool ByteArray::readFromFile(const std::string& name){
        std::ifstream ifs;
        ifs.open(name, std::ios::binary);   //以二进制打开
        if(!ifs){
            BIN_LOG_ERROR(g_logger) << "readFromFile name=" << name << " error, errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }

        //用智能指针定义一个char数组 指定析构函数释放该空间
        std::shared_ptr<char> buff(new char[m_baseSize], [](char* ptr){ delete[] ptr;});
        while(!ifs.eof()){  //用智能指针定义一个char数组 指定析构函数释放该空间
            //从文件读取到buf 每次读取一个结点大小的数据量
            ifs.read(buff.get(), m_baseSize);
            //从buf写回到内存池  gcount()是真正的从文件读取到的长度
            write(buff.get(), ifs.gcount());
        }
        return true;
    }

    void ByteArray::addCapacity(size_t size){
        if(size == 0)
            return;
        
        size_t old_cap = getCapacity(); //获取当前的可写入容量(m_capacity - m_position)，比size大大就不扩容
        if(old_cap >= size)
            return;
        
        size = size - old_cap;  //减去原有的容量
        size_t count = ceil(1.0 * size / m_baseSize);   //计算需额外添加结点的数量

        //找到尾部内存块结点的位置
        Node* tmp = m_root;
        while(tmp->next) 
            tmp = tmp->next;
        
        //从尾部开始尾插法添加新结点
        Node* first = NULL;
        for(size_t i = 0; i < count; ++i){
            tmp->next = new Node(m_baseSize);
            if(first == NULL)
                first = tmp->next;  //将新加入的第一个结点记录一下便于连接
            tmp = tmp->next;
            m_capacity += m_baseSize;
        }

        //如果原来的容量恰好用完 要把m_cur置到第一个新结点上
        if(old_cap == 0)
            m_cur = first;
    }

    std::string ByteArray::toString() const{
        std::string str;
        str.resize(getReadSize());
        if(str.empty())
            return str;
        read(&str[0], str.size(), m_position);  //不影响内存指针位置 仅仅是显示
        return str;
    }

    std::string ByteArray::toHexString() const{
        std::string str = toString();
        std::stringstream ss;

        for(size_t i = 0; i < str.size(); ++i){
            if(i > 0 && i % 32 == 0)    //一行显示32个字符
                ss << std::endl;
            //一次显示一个字节 2个字符，不足的要用0补齐
            ss << std::setw(2) << std::setfill('0') << std::hex << (int)(uint8_t)str[i] << " ";
        }

        return ss.str();
    }


    uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const{
        len = len > getReadSize() ? getReadSize() : len;
        if(len == 0)
            return 0;

        uint64_t size = len;

        size_t npos = m_position % m_baseSize;
        size_t ncap = m_cur->size - npos;
        struct iovec iov;
        Node* cur = m_cur;

        while(len > 0){
            if(ncap >= len){    //当前结点剩余容量大小 > 读取的长度
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = len;
                len = 0;
            }else{  // 当前结点剩余容量大小 <= 读取的长度
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = ncap;
                len -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
            buffers.push_back(iov);
        }
        return size;
    }

    uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const{
        len = len > getReadSize() ? getReadSize() : len;
        if(len == 0)
            return 0;

        uint64_t size = len;

        size_t npos = position % m_baseSize;
        size_t count = position / m_baseSize;
        Node* cur = m_root;
        while(count > 0){
            cur = cur->next;
            --count;
        }

        size_t ncap = cur->size - npos;
        struct iovec iov;
        while(len > 0){
            if(ncap >= len){
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = len;
                len = 0;
            }else{
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = ncap;
                len -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
            buffers.push_back(iov);
        }
        return size;
    }

    uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len){
        if(len == 0){
            return 0;
        }
        addCapacity(len);
        uint64_t size = len;

        size_t npos = m_position % m_baseSize;
        size_t ncap = m_cur->size - npos;
        struct iovec iov;
        Node* cur = m_cur;
        while(len > 0){
            if(ncap >= len){
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = len;
                len = 0;
            }else{
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = ncap;

                len -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
            buffers.push_back(iov);
        }
        return size;
    }

}
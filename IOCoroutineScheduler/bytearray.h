//二进制数组(序列化/反序列化)
#ifndef __SYLAR_BYTEARRAY_H__
#define __SYLAR_BYTEARRAY_H__

#include <memory>
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>

namespace bin {
    /*//skip:
        protobuf: 语雀有详细说明
    */
    /*
    //power:memcpy函数
            varint：保留7位writeUint32、readUint32函数
            存和取的时候要检测字节序
    */

    //二进制数组,提供基础类型的序列化,反序列化功能
    class ByteArray {
    public:
        typedef std::shared_ptr<ByteArray> ptr;

        //ByteArray的存储节点
        struct Node {
            Node(size_t s); //构造指定大小的内存块  s: 内存块字节数
            Node();
            ~Node();

            char* ptr;      //内存块地址指针
            Node* next;     //下一个内存块地址
            size_t size;    //内存块大小
        };

        ByteArray(size_t base_size = 4096);
        ~ByteArray();
        
        /*//block：正常编码：m_position += sizeof(value)，如果m_position > m_size 则 m_size = m_position
            除了1字节长度的数据，其他 >1字节的数据需要检查存储的数据字节序是否和用户物理机的字节序相同，不同需要进行字节序调整交换*/
        void writeFint8  (int8_t value);    //写入固定长度int8_t类型的数据
        void writeFuint8 (uint8_t value);   //写入固定长度uint8_t类型的数据
        void writeFint16 (int16_t value);   //写入固定长度int16_t类型的数据(大端/小端)
        void writeFuint16(uint16_t value);  //写入固定长度uint16_t类型的数据(大端/小端)
        void writeFint32 (int32_t value);   //写入固定长度int32_t类型的数据(大端/小端)
        void writeFuint32(uint32_t value);  //写入固定长度uint32_t类型的数据(大端/小端)
        void writeFint64 (int64_t value);   //写入固定长度int64_t类型的数据(大端/小端)
        void writeFuint64(uint64_t value);  //写入固定长度uint64_t类型的数据(大端/小端)

        //block：varint编码：，如果m_position > m_size 则 m_size = m_position
        void writeInt32  (int32_t value);   //写入有符号Varint32类型的数据，m_position += 实际占用内存(1 ~ 5)
        void writeUint32 (uint32_t value);  //写入无符号Varint32类型的数据，m_position += 实际占用内存(1 ~ 5)
        void writeInt64  (int64_t value);   //写入有符号Varint64类型的数据，m_position += 实际占用内存(1 ~ 10)
        void writeUint64 (uint64_t value);  //写入无符号Varint64类型的数据，m_position += 实际占用内存(1 ~ 10)
        void writeFloat  (float value);     //写入float类型的数据，m_position += sizeof(value)
        void writeDouble (double value);    //写入double类型的数据，m_position += sizeof(value)
        void writeStringF16(const std::string& value);  //写入std::string类型的数据,用uint16_t作为长度类型，m_position += 2 + value.size()
        void writeStringF32(const std::string& value);  //写入std::string类型的数据,用uint32_t作为长度类型，m_position += 4 + value.size()
        void writeStringF64(const std::string& value);  //写入std::string类型的数据,用uint64_t作为长度类型，m_position += 8 + value.size()
        void writeStringVint(const std::string& value); //写入std::string类型的数据,用无符号Varint64作为长度类型，m_position += Varint64长度 + value.size()
        void writeStringWithoutLength(const std::string& value);    //写入std::string类型的数据,无长度，m_position += value.size()

        //block：读取固定长度数据
        /*除了1字节长度的数据，其他 >1字节的数据需要检查存储的数据字节序是否和用户物理机的字节序相同，不同需要进行字节序调整交换
        如果getReadSize() < sizeof(uint64_t) 抛出 std::out_of_range
        运行后：m_position += sizeof(数据类型);*/
        int8_t   readFint8();           //读取int8_t类型的数据
        uint8_t  readFuint8();          //读取uint8_t类型的数据
        int16_t  readFint16();          //读取int16_t类型的数据
        uint16_t readFuint16();         //读取uint16_t类型的数据
        int32_t  readFint32();          //读取int32_t类型的数据
        uint32_t readFuint32();         //读取uint32_t类型的数据
        int64_t  readFint64();          //读取int64_t类型的数据
        uint64_t readFuint64();         //读取uint64_t类型的数据

        //block：写入varint压缩编码数据
        /*调用前：getReadSize() >= Varint数（16/32/64/float/double)实际占用内存，否则抛出 std::out_of_range
                 getReadSize() >= VarintString（16/32/64/float/double)+size 实际占用内存，否则抛出 std::out_of_range
          调用后:：m_position += Varint数实际占用内存
                  m_position += VarintString实际占用内存 */
        int32_t     readInt32();        //读取有符号Varint32类型的数据
        uint32_t    readUint32();       //读取无符号Varint32类型的数据
        int64_t     readInt64();        //读取有符号Varint64类型的数据
        uint64_t    readUint64();       //读取无符号Varint64类型的数据
        float       readFloat();        //读取float类型的数据
        double      readDouble();       //读取double类型的数据
        std::string readStringF16();    //读取std::string类型的数据,用uint16_t作为长度
        std::string readStringF32();    //读取std::string类型的数据,用uint32_t作为长度
        std::string readStringF64();    //读取std::string类型的数据,用uint64_t作为长度
        std::string readStringVint();   //读取std::string类型的数据,用无符号Varint64作为长度

        void clear();   //清空ByteArray

        //block：核心函数
        //向内存缓存buf中写入size长度的数据。m_position += size, 如果m_position > m_size 则 m_size = m_position
        void write(const void* buf, size_t size);
        //在内存缓存buf中读取size长度的数据
        //m_position += size, 如果m_position > m_size 则 m_size = m_position。如果getReadSize() < size 则抛出 std::out_of_range
        void read(void* buf, size_t size);
        //在内存缓存buf中读取size长度的数据，从position开始读取。如果 (m_size - position) < size 则抛出 std::out_of_range
        void read(void* buf, size_t size, size_t position) const;

        size_t getPosition() const { return m_position;}            //返回ByteArray当前位置
        void setPosition(size_t val);                               //设置当前操作内存指针的位置
                
        bool writeToFile(const std::string& name) const;            //把ByteArray的数据写入到文件中 name 文件名
        bool readFromFile(const std::string& name);                 //从文件中读取数据  name: 文件名

        size_t getBaseSize() const { return m_baseSize;}            //返回内存块的大小
        size_t getReadSize() const { return m_size - m_position;}   //返回可读取数据大小
        size_t getSize() const { return m_size;}                    //返回数据的长度
        
        bool isLittleEndian() const;        //是否是小端
        void setIsLittleEndian(bool val);   //设置是否为小端

        std::string toString() const;       //将ByteArray里面的数据[m_position, m_size)转成std::string
        std::string toHexString() const;    //将ByteArray里面的数据[m_position, m_size)转成16进制的std::string(格式:FF FF FF)

        //获取所有可读取的缓存,写入长度为len的数据保存到iovec数组buffers，返回实际数据的长度
        //len：读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
        uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;
        //获取所有可读取的缓存,写入长度为len的数据保存到iovec数组buffers,从position开始，返回实际数据的长度
        //len：读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
        uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;
        //获取所有可写入的缓存,写入长度为len的数据保存到iovec数组buffers中，返回实际的长度
        //len：写入的长度，如果(m_position + len) > m_capacity 则 m_capacity扩容N个节点以容纳len长度
        uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);
        
    private:
        void addCapacity(size_t size);  //核心函数：给内存扩容(ByteArray),使其可容纳size个数据(如果原本可容纳size个,则不扩)
        size_t getCapacity() const { return m_capacity - m_position;}   //获取当前的可写入容量

    private:
        size_t m_baseSize;  //内存块的大小
        size_t m_position;  //当前操作位置，N * m_baseSize + nowposition
        size_t m_capacity;  //当前的总容量
        size_t m_size;      //当前数据的大小
        int8_t m_endian;    //字节序,默认大端
        Node* m_root;       //第一个内存块指针
        Node* m_cur;        //当前操作的内存块指针
    };
}

#endif
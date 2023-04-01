//字节序操作函数(大端/小端)

#ifndef __BIN_ENDIAN_H__
#define __BIN_ENDIAN_H__

#define BIN_LITTLE_ENDIAN 1   //小端是1
#define BIN_BIG_ENDIAN 2      //大端是2

#include <byteswap.h>
#include <stdint.h>

namespace bin{

    //add: SFINEA，替换失败并不是一个错误，即subsitution failure is not an error
    /*使用SFINEA规则对不同位数据的位交换重载：
    template <bool, typename T=void>
    struct enable_if {
    };
    
    template <typename T>
    struct enable_if<true, T> {
    using type = T;
    };
    */

    /*功能：对16bit、32bit、64bit的数据重载它们的高低位交换函数。
         实际上通过反汇编文件，使用if...else写法和这种模板函数重载效果是一样的
    */   

    //8字节类型的字节序转化
    template<class T>
    typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
    byteswap(T value){
        return (T)bswap_64((uint64_t)value);
    }

    //4字节类型的字节序转化
    template<class T>
    typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
    byteswap(T value){
        return (T)bswap_32((uint32_t)value);
    }

    //2字节类型的字节序转化
    template<class T>
    typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
    byteswap(T value){
        return (T)bswap_16((uint16_t)value);
    }



    /*根据用户物理机大小端存储，决定如何转换字节序
    功能：网络字节序一般指的大端字节序。
        小端机器：收到大端字节序-------->小端字节序；发出：小端字节序------>大端字节序
        大端机器：收到：一般不需要做改变；发出：一般不需要做改变
    */

    #if BYTE_ORDER == BIG_ENDIAN
    #define BIN_BYTE_ORDER BIN_BIG_ENDIAN
    #else
    #define BIN_BYTE_ORDER BIN_LITTLE_ENDIAN
    #endif



    #if BIN_BYTE_ORDER == BIN_BIG_ENDIAN //判断物理机大小端
    //得到大端 大端机器 什么都不用操作
    //只在小端机器上执行byteswap, 在大端机器上什么都不做
    template<class T>
    T byteswapOnLittleEndian(T t){
        return t;
    }

    //得到小端 大端机器 大端----->小端
    //只在大端机器上执行byteswap, 在小端机器上什么都不做
    template<class T>
    T byteswapOnBigEndian(T t){
        return byteswap(t);
    }

    #else
    //得到大端 小端机器 小端------->大端
    //只在小端机器上执行byteswap, 在大端机器上什么都不做
    template<class T>
    T byteswapOnLittleEndian(T t){
        return byteswap(t);
    }

    //得到小端 小端机器 什么都不用做
    //只在大端机器上执行byteswap, 在小端机器上什么都不做
    template<class T>
    T byteswapOnBigEndian(T t){
        return t;
    }
    #endif

}

#endif

//常用宏的封装
#ifndef __SYLAR_MACRO_H__
#define __SYLAR_MACRO_H__

#include <string.h>
#include <assert.h>
#include "log.h"
#include "util.h"


//power: #if defined ...  和#if !defined ... 
/*
一、
    #ifdef __GNUC__ || defined __llvm__
    #   define SYLAR_LIKELY(x)       __builtin_expect(!!(x), 1)
    #   define SYLAR_UNLIKELY(x)     __builtin_expect(!!(x), 0)
    #else
    #   define SYLAR_LIKELY(x)      (x)
    #   define SYLAR_UNLIKELY(x)      (x)
    #endif

    但上述方法是错误的。因为ifdef和ifndef仅能跟一 个宏定义 参数，而不能使用表达式。此时可考虑使用
        #if defined
            和
        #if !defined

二、
    __GNUC__，gcc定义的宏，三个分别可以表示gcc主版本，次版本号，修正版本号
    __llvm__和__clang__macros是分别检查LLVM编译器（llvm-gcc或clang）或clang的官方方式
*/
#if defined __GNUC__ || defined __llvm__
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#   define SYLAR_LIKELY(x)       __builtin_expect(!!(x), 1)
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#   define SYLAR_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#   define SYLAR_LIKELY(x)      (x)
#   define SYLAR_UNLIKELY(x)      (x)
#endif




// 断言宏封装
#define SYLAR_ASSERT(x) \
    if(SYLAR_UNLIKELY(!(x))){ \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x \
            << "\nbacktrace:\n" \
            << sylar::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

// 断言宏封装
#define SYLAR_ASSERT2(x, w) \
    if(SYLAR_UNLIKELY(!(x))){ \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << sylar::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif

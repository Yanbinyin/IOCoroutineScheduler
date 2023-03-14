#include "../server-bin/bin.h"
#include <assert.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

/*
    assert宏的原型定义在<assert.h>中，其原型定义：
    #include <assert.h>
    void assert( int expression );

    assert的作用是现计算表达式 expression，如果其值为假（即为0），那么它先向stderr打印一条出错信息，
    然后通过调用 abort 来终止程序运行。
*/
void test_assert(){
    // SYLAR_LOG_INFO(g_logger) << sylar::BacktraceToString(10, 0, "  ");
    SYLAR_LOG_INFO(g_logger) << sylar::BacktraceToString(10);   //默认形参，skip = 2，跳两层，相比上面输出简洁清爽
    
    //测试单纯assert
    // assert(0);
    
    //测试封装的的assert
    // SYLAR_ASSERT(false);
    SYLAR_ASSERT2(0 == 1, "abcdef xx");
}

int main(){
    test_assert();
    return 0;
}
#include "../server-bin/bin.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
// static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

//run_in_fiber()函数，所有测试main函数都要用
void run_in_fiber(){
    // printPostfix("run_in_fiber begin:");
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber begin";
    sylar::Fiber::YieldToHold();
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber back";
    sylar::Fiber::YieldToHold();
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber end";
}


//block1: ：测试1、测试时注释掉其他的
//power:协程构造数量 ！= 析构数量，只有两次换入，两次换出，run_in_fiber()未执行完
#if 0
int main(){
    sylar::Thread::SetName("mainThread");
    sylar::Fiber::GetThis();
    SYLAR_LOG_INFO(g_logger) << "main begin";

    sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));

    // //测试查看run_in_fiber()输出的内容
    // run_in_fiber();
    printLine();

    fiber->swapIn();
    SYLAR_LOG_INFO(g_logger) << "main after swapIn";

    printLine();

    fiber->swapIn();
    SYLAR_LOG_INFO(g_logger) << "main after end";

    return 0;
}
#endif


//block2：协程构造数量 = 析构数量
#if 1
int main(){
    SYLAR_LOG_INFO(g_logger) << "main begin -1";
    {
        sylar::Fiber::GetThis();
        SYLAR_LOG_INFO(g_logger) << "main begin";
        sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));

        printLine();
        fiber->swapIn();
        SYLAR_LOG_INFO(g_logger) << "main after swapIn";

        printLine();
        fiber->swapIn();
        SYLAR_LOG_INFO(g_logger) << "main after end";

        printLine();
        fiber->swapIn();
    }
    SYLAR_LOG_INFO(g_logger) << "main after end2";

    return 0;
}
#endif


//block3：多线程下协程的切换： 协程构造数量 = 析构数量
#if 0
void test_fiber(){
    SYLAR_LOG_INFO(g_logger) << "main begin -1";
    {
        sylar::Fiber::GetThis();
        SYLAR_LOG_INFO(g_logger) << "main fiber begin";
        sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));

        int i = 0;
        SYLAR_LOG_INFO(g_logger) << "fiber begin swapIn"  << std::to_string(++i);
        fiber->swapIn();
        //SYLAR_LOG_INFO(g_logger) << "fiber after swapIn";

        SYLAR_LOG_INFO(g_logger) << "fiber begin swapIn"  << std::to_string(++i);
        fiber->swapIn();
        //SYLAR_LOG_INFO(g_logger) << "fiber after end";

        SYLAR_LOG_INFO(g_logger) << "fiber begin swapIn"  << std::to_string(++i);
        fiber->swapIn();
    }
    SYLAR_LOG_INFO(g_logger) << "main after end2";
}

int main(){
    sylar::Thread::SetName("mainThread");

    std::vector<sylar::Thread::ptr> thrs;
    for(int i = 0; i < 2; ++i){
        sylar::Thread::ptr thr(new sylar::Thread(&test_fiber, "name_" + std::to_string(i)));
        thrs.push_back(sylar::Thread::ptr(thr));
    }

    printLine("join() begins waiting for thrs to end:", 80);
    //int index = 0;
    for(auto thr : thrs){
        thr->join();
        //printLine("join() ends waiting: " + std::to_string(++index), 80);
    }

    return 0;
}
#endif
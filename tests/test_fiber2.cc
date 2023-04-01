#include "IOCoroutineScheduler/bin.h"

bin::Logger::ptr g_logger = BIN_LOG_ROOT();

void run_in_fiber(){
    BIN_LOG_INFO(g_logger) << "run_in_fiber begin";
    bin::Fiber::YieldToHold();
    BIN_LOG_INFO(g_logger) << "run_in_fiber end";
    bin::Fiber::YieldToHold();
}

void test_fiber(){
    BIN_LOG_INFO(g_logger) << "main begin -1";
    {
        bin::Fiber::GetThis();
        BIN_LOG_INFO(g_logger) << "main begin";
        bin::Fiber::ptr fiber(new bin::Fiber(run_in_fiber));
        fiber->swapIn();
        BIN_LOG_INFO(g_logger) << "main after swapIn";
        fiber->swapIn();
        BIN_LOG_INFO(g_logger) << "main after end";
        fiber->swapIn();
    }
    BIN_LOG_INFO(g_logger) << "main after end2";
}

int main(int argc, char** argv){
    bin::Thread::SetName("main");

    std::vector<bin::Thread::ptr> thrs;
    for(int i = 0; i < 3; ++i){
        thrs.push_back(bin::Thread::ptr(
                    new bin::Thread(&test_fiber, "name_" + std::to_string(i))));
    }
    for(auto i : thrs){
        i->join();
    }
    return 0;
}

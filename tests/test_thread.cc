#include "IOCoroutineScheduler/bin.h"
#include <unistd.h>

bin::Logger::ptr g_logger = BIN_LOG_ROOT();

int count = 0;
int countcmp = 0;

bin::RWMutex s_mutex;

// block1:检测锁在多线程中的保护作用
#if 0
void fun1(){
    for(int i = 0; i < 100000; ++i){
        bin::RWMutex::WriteLock lock(s_mutex);
        ++count;
    }
    for(int i = 0; i < 100000; ++i){
        ++countcmp;
    }
    //std::cout不是线程安全的，可能会乱行   
    std::cout << "count = " << count << ", countcmp" << countcmp << std::endl;
}

void test1(){
    std::vector<bin::Thread::ptr> thrs;   //创建线程池
    for(int i = 0; i < 50; ++i){
        bin::Thread::ptr thr(new bin::Thread(&fun1, "name_" + std::to_string(i)));
        thrs.push_back(thr);
    }

    for(auto& i : thrs)
        i->join();

    BIN_LOG_INFO(g_logger) << "count = " << count;
    BIN_LOG_INFO(g_logger) << "countcmp = " << countcmp;  //有一定概率出错
}

int main(){
    BIN_LOG_INFO(g_logger) << "thread test1() begin, 检测锁在多线程中的保护作用";
    test1();
    printL();
    return 0;
}
#endif

// block2：测试子线程和父线程的执行关系：并行执行
#if 1
void fun2() {
  BIN_LOG_INFO(g_logger) << "running thread name: " << bin::Thread::GetName()
                         << ", this.name: " << bin::Thread::GetThis()->getName()
                         << ", running thread id: " << bin::GetThreadId()
                         << ", this.id: " << bin::Thread::GetThis()->getId()
                         << ", m_thread: "
                         << bin::Thread::GetThis()->getThread();
  /*添加睡眠，可以用终端查看进程
      ps uax | grep thread
      top -H -p 11726
  */
  sleep(100);
}

void test2() {
  std::vector<bin::Thread::ptr> thrs; // 创建线程池
  for (int i = 0; i < 5; ++i) {
    // power:初始化完成，子线程便会执行，和父线程分属两个分支执行
    bin::Thread::ptr thr(new bin::Thread(&fun2, "name_" + std::to_string(i)));

    sleep(0.1); // 睡眠一下，等子线程执行输出
    BIN_LOG_INFO(g_logger) << "thread " << i << " 初始化完成";

    thrs.push_back(thr);
  }

  printLine("join:");
  for (auto &i : thrs)
    i->join(); // power:join()等待所有子线程结束，join和析构函数的detach只会执行一个
  BIN_LOG_INFO(g_logger) << "test2() end";
}

int main() {
  BIN_LOG_INFO(g_logger)
      << "thread test2() begin, 测试子线程和父线程的执行关系";
  test2();
  BIN_LOG_INFO(g_logger) << "main() end";
  return 0;
}
#endif
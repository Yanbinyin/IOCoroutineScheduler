/**
 * @file coroutine.h
 * @author yinyb (990900296@qq.com)
 * @brief 协程调度器封装类
 * @version 1.0
 * @date 2022-04-04
 * @copyright Copyright (c) {2022}
 */

/*
 * 协程调度器,封装的是N-M的协程调度器，实现跨线程间的协程切换
 * scheduler  --> thread  --> fiber
 *     1 - N           1 - M
 *             N - M
 * 实现目标：
 *   实现跨线程间的协程切换。让A线程中的1号协程切换到B线程中的3号协程上去执行对应的任务。即：
 *   线程间从任务队列"抢"任务，抢到任务之后以协程为目标代码载体，使用同步切换的方式到对应的目
 *   标代码上进行执行。
 * 非常非常注意：
 *   采用ucontext的API，如果程序上下文衔接不恰当，会导致最后一个协程退出是时候，整个主线程也
 *   退出了，这是相当危险的！！！
 * 实现调度器schedule
 *   分配有多个线程，一个线程又有分配多个协程。 N个线程对M个协程。
 *   1. schedule 是一个线程池，分配一组线程。
 *   2. schedule 是一个协程调度器，分配协程到相应的线程去执行目标代码
 *   a. 方式一：协程随机选择一个空闲的任意的线程上执行
 *   b. 方式二：给协程指定一个线程去执行
 */

#ifndef __BIN_SCHEDULER_H__
#define __BIN_SCHEDULER_H__

#include <iostream>
#include <list>
#include <memory>
#include <vector>

#include "coroutine.h"
#include "thread.h"

namespace bin {

class Scheduler {
  typedef std::shared_ptr<Scheduler> ptr;
  typedef Mutex MutexType;

public:
  /**
   * @brief Construct a new Scheduler object
   * 
   * @param threads 线程数量
   * @param use_caller 当前线程是否纳入调度队列，默认纳入调度
   * @param name 协程调度器名称
   */
  Scheduler(size_t threads = 1, bool use_caller = true,
            const std::string &name = "");
  virtual ~Scheduler();

  const std::string &getName() const { return m_name; }

  static Scheduler *GetThis(); // 返回当前协程调度器，如果没有，创建第一个协程
  static Fiber *GetMainFiber(); // 返回当前协程调度器的调度协程

  void start(); // 启动协程调度器
  void stop();  // 停止协程调度器

  /**
   * @brief 
   * @tparam FiberOrCb 添加任务函数模板：单个调度协程
   * @param fc 协程或函数
   * @param thread 协程执行的线程id,-1标识任意线程
   */
  template <class FiberOrCb> void schedule(FiberOrCb fc, int thread = -1) {
    bool need_tickle = false;
    {
      MutexType::Lock lock(m_mutex);
      need_tickle = scheduleNoLock(fc, thread);
    }
    if (need_tickle) {
      tickle();
    }
  }

  /**
   * @brief 添加任务函数模板，批量调度协程
   * @tparam InputIterator 迭代器类型
   * @param begin 协程数组的开始
   * @param end 协程数组的结束
   */
  template <class InputIterator>
  void schedule(InputIterator begin, InputIterator end) {
    bool need_tickle = false;
    {
      MutexType::Lock lock(m_mutex);
      while (begin != end) {
        need_tickle = scheduleNoLock(&*begin, -1) || need_tickle;
        ++begin;
      }
    }
    if (need_tickle) {
      tickle();
    }
  }

  void switchTo(int thread = -1);
  std::ostream &dump(std::ostream &os);

protected:
  void setThis(); // 设置当前的协程调度器
  bool hasIdleThreads() { return m_idleThreadCount > 0; } // 是否有空闲线程
  void run();                                             // 协程调度函数

  virtual void tickle(); // 线程唤醒，通知协程调度器有任务了，派生类中实现
  virtual bool stopping(); // 线程清理回收，返回是否可以停止

  // 协程无任务可调度时执行idle协程，借助epoll_wait来唤醒有任务可执行
  virtual void idle();

private:
  /**
   * @brief 添加任务函数模板，真正执行添加动作
   * @tparam FiberOrCb
   * @param fc 协程或函数
   * @param thread 协程执行的线程id,-1标识任意线程
   * @return 是否需要tickle
   */
  template <class FiberOrCb> bool scheduleNoLock(FiberOrCb fc, int thread) {
    bool need_tickle = m_fibers.empty();
    FiberAndThread ft(fc, thread);
    if (ft.fiber || ft.cb) {
      m_fibers.push_back(ft);
    }
    return need_tickle;
  }

private:
  /**
   * @brief 封装一个自定义的可执行对象结构体，来封装 协程/函数/线程组
   * @details 调度器调度执行的不仅为Fiber协程体，还可以是一个function可调用对象
   */
  struct FiberAndThread {
    Fiber::ptr fiber;         /// 协程
    std::function<void()> cb; /// 协程执行函数
    int thread;               /// 线程id

    /**
     * @brief 构造函数，f协程在thr这个线程上运行
     * @param[in] f 协程
     * @param[in] thr 线程id
     */
    FiberAndThread(Fiber::ptr f, int thr) : fiber(f), thread(thr) {}

    /**
     * @brief 构造函数
     * @param[in] f 协程指针
     * @param[in] thr 线程id
     * @post *f = nullptr
     */
    FiberAndThread(Fiber::ptr *f, int thr) : thread(thr) {
      /*
       * smart pointer(SP)本质是使用局部变量生命周期管理heap memory.上一个
       * constructor 直接传递SP对象，该对象位于stack，stack
       * variable只要执行到花括号就会释放，所以它不
       * 需要swap，就会自动destruct，引用计数自动-1；
       * 但是第二个传递的SP to
       * SP，SP对象本身在堆区，不会自动调用destructor。也就是说，只
       * 要外部没有由程序员主动析构，引用计数至少为1；
       * 但是一般不推荐这样搞，最好不要试图接管外部传递过来的指针的生命周期，同时非特殊情况不要
       * 用SP to SP，违背了SP利用局部变量生命周期管理heap内存的初衷
       */
      fiber.swap(*f); // 减少一次智能指针引用
    }

    /**
     * @brief 构造函数
     * @param[in] f 协程执行函数
     * @param[in] thr 线程id
     */
    FiberAndThread(std::function<void()> f, int thr) : cb(f), thread(thr) {}

    /**
     * @brief 构造函数
     * @param[in] f 协程执行函数指针
     * @param[in] thr 线程id
     * @post *f = nullptr
     */
    FiberAndThread(std::function<void()> *f, int thr) : thread(thr) {
      cb.swap(*f); // 减少一次智能指针引用
    }

    /**
     * @brief 无参构造函数,和STL结合必须有默认构造函数，不然分配的对象无法初始化
     */
    FiberAndThread() : thread(-1) {}

    /**
     * @brief 重置数据
     */
    void reset() {
      fiber = nullptr;
      cb = nullptr;
      thread = -1;
    }
  };

protected:
  size_t m_threadCount = 0;                   /// 线程总数
  std::vector<Thread::ptr> m_threads;         /// 线程池
  std::vector<int> m_threadIds;               /// 线程id数组(除主线程)
  std::atomic<size_t> m_activeThreadCount{0}; /// 工作线程数量
  std::atomic<size_t> m_idleThreadCount{0};   /// 空闲线程数量
  Fiber::ptr m_rootFiber; /// use_caller=true时，调度器所在线程的调度协程
  int m_rootThread = 0; /// use_caller=true时，调度器所在线程的线程id
  bool m_stopping = true;  /// 是否正在停止
  bool m_autoStop = false; /// 是否自动停止

private:
  MutexType m_mutex;                  /// 锁
  std::string m_name;                 /// 协程调度器名称
  std::list<FiberAndThread> m_fibers; /// 协程任务队列
};

class SchedulerSwitcher : public Noncopyable {
public:
  SchedulerSwitcher(Scheduler *target = nullptr);
  ~SchedulerSwitcher();

private:
  Scheduler *m_caller;
};

} // namespace bin

#endif

/**
 * @file coroutine.cc
 * @author yinyb (990900296@qq.com)
 * @brief 协程调度器封装类
 * @version 1.0
 * @date 2022-04-04
 * @copyright Copyright (c) {2022}
 */

#include "scheduler.h"
#include "hook.h"
#include "log.h"
#include "macro.h"

namespace bin {

static bin::Logger::ptr g_logger = BIN_LOG_NAME("system");

// t_scheduler_fiber保存当前线程的调度协程，加上Fiber模块的t_fiber和t_thread_fiber，
// 每个线程总共可以记录三个协程的上下文信息

/// 当前线程的调度器，同一个调度器下的所有线程指同同一个调度器实例
static thread_local Scheduler *t_scheduler = nullptr;

/// 当前线程的调度协程，每个线程都独有一份，包括caller线程
static thread_local Fiber *t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
    : m_name(name) {
  BIN_LOG_DEBUG(g_logger) << "调度器构造: Scheduler " << name;
  BIN_ASSERT(threads > 0); // 线程数量要 ≥ 1
  // 当前线程作为调度线程使用
  if (use_caller) {
    bin::Fiber::GetThis(); // 初始化母协程
    --threads; // 减1是因为当前的这个线程也会被纳入调度 少创建一个线程
    // 这个断言防止重复创建调度器
    BIN_ASSERT(GetThis() == nullptr);
    t_scheduler = this;
    // 新创建的主协程会参与到协程调度中
    m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
    bin::Thread::SetName(m_name); // 防止线程名称没改
    // 线程的主协程不再是一开始使用协程初始化出来的那个母协程，而应更改为创建了调度器的协程
    t_scheduler_fiber = m_rootFiber.get();
    m_rootThread = bin::GetThreadId();
    m_threadIds.push_back(m_rootThread);
  } else { // 当前线程不作为调度线程使用
    m_rootThread = -1;
  }
  m_threadCount = threads;
}

Scheduler::~Scheduler() {
  // 只有正在停止运行才能析构
  BIN_ASSERT(m_stopping);
  if (GetThis() == this) {
    t_scheduler = nullptr;
  }
  BIN_LOG_DEBUG(g_logger) << "调度器析构: ~Scheduler";
}

Scheduler *Scheduler::GetThis() { return t_scheduler; }

Fiber *Scheduler::GetMainFiber() { return t_scheduler_fiber; }

// 核心函数:开启Schuduler调度器的运行。根据传入的线程数，初始化其余子线程，将调度协程推送到CPU
void Scheduler::start() {
  BIN_LOG_INFO(g_logger) << "Scheduler::start()";
  MutexType::Lock lock(m_mutex);
  // 一开始 m_stopping = true
  // 是运行状态，直接返回
  if (!m_stopping)
    return;
  // 是停止状态，开始运行
  m_stopping = false;
  BIN_ASSERT(m_threads.empty());
  m_threads.resize(m_threadCount);
  for (size_t i = 0; i < m_threadCount; ++i) {
    // BIN_LOG_INFO(g_logger) << "创建线程"; //power:开启线程
    // hack: 协程是Scheduler::run，线程还是Scheduler::run
    m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this),
                                  m_name + "_" + std::to_string(i)));
    m_threadIds.push_back(m_threads[i]->getId());
  }
  lock.unlock();
  // if(m_rootFiber){
  //     //m_rootFiber->swapIn();
  //     m_rootFiber->call();
  //     BIN_LOG_INFO(g_logger) << "call out " << m_rootFiber->getState();
  // }
}

// power:核心函数，调度器停止
void Scheduler::stop() {
  BIN_LOG_INFO(g_logger) << "Scheduler::stop()";
  /*
   *  用了use_caller的线程 必须在这个线程里去执行stop
   *  没有用use_caller的线程 可以任意在别的线程执行stop
   */
  m_autoStop = true;
  // 1.只有一个主线程在运行的情况  直接停止即可
  // 只有一个线程（即：主线程/调度协程在运行），并且调度协程处于终止态或创建态。直接调stopping()负责清理、回收工作，退出返回
  if (m_rootFiber && m_threadCount == 0 &&
      (m_rootFiber->getState() == Fiber::TERM ||
       m_rootFiber->getState() == Fiber::INIT)) {
    BIN_LOG_INFO(g_logger) << this << " stopped";
    m_stopping = true;
    if (stopping())
      return;
  }
  // 2.多个线程在运行  先把子线程停止  再停止主线程
  // 有多个线程（是一组线程池），先设置标志位m_stopping，唤醒tick0le()其他子线程根据该标志
  // 位退出；都完毕之后，再将调度协程唤醒退出
  if (m_rootThread != -1) { // 主线程Id不为-1说明是创建调度器(use_caller)的线程
    // 当前的执行器要把创建它的线程也使用的时候  它的stop一定要在创建线程中执行
    BIN_ASSERT(GetThis() == this);
  } else {
    BIN_ASSERT(GetThis() != this);
  }
  // 其他线程根据这个标志位退出运行
  m_stopping = true;
  // 唤醒其他线程结束
  for (size_t i = 0; i < m_threadCount; ++i) {
    BIN_LOG_INFO(g_logger) << "唤醒其他线程: " << i;
    tickle();
  }
  // 最后让主线程也退出
  if (m_rootFiber) {
    BIN_LOG_INFO(g_logger) << "唤醒主线程";
    tickle();
  }
  if (m_rootFiber) {
    // while(!stopping()){
    //     if(m_rootFiber->getState() == Fiber::TERM
    //             || m_rootFiber->getState() == Fiber::EXCEPT){
    //         m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0,
    //         true)); BIN_LOG_INFO(g_logger) << " root fiber is term, reset";
    //         t_fiber = m_rootFiber.get();
    //     }
    //     m_rootFiber->call();
    // }
    if (!stopping()) {
      BIN_LOG_INFO(g_logger) << "call";
      m_rootFiber->call();
    }
  }
  // 创建中间变量thrs，thrs出了作用域就析构，里面智能指针也析构
  std::vector<Thread::ptr> thrs;
  {
    MutexType::Lock lock(m_mutex);
    thrs.swap(m_threads);
  }
  for (auto &i : thrs) {
    i->join();
  }
}

void Scheduler::setThis() { t_scheduler = this; }

/*//power:核心函数：

*/
/*
 * 两个部分在执行该函数：一部分是负责在线程里处理调度工作的主协程，一部分是线程池的其他子线程
 * 功能：负责协程调度和线程管理，从任务队列拿出任务，执行对应的任务。
 */
void Scheduler::run() {
  BIN_LOG_DEBUG(g_logger) << "Scheduler::run() begin";
  set_hook_enable(true);
  setThis();
  // 当前线程ID不等于主线程ID
  if (bin::GetThreadId() != m_rootThread) { // 当前线程未作为调度线程使用？？？
    t_scheduler_fiber = Fiber::GetThis().get();
  }
  // power:创建一个专门跑idel()的协程，调度任务都完成之后去做idle
  Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
  Fiber::ptr cb_fiber; // 回调函数，function函数的协程
  FiberAndThread ft;
  /*● 核心逻辑：
   * while(1){
   * 1.加锁，取出可执行对象容器/消息队列/任务队列m_fibers中的元素，解锁
   * a.如果当前的可执行对象没有指定线程且不是当前在跑的线程要执行的，就跳过。并且设置一个
   * bool信号量，如is_tickle以便通知其他线程来执行这个属于它们的可执行对象（任务）
   * b.如果当前的可执行对象是当前在跑的线程要执行的，检查协程体和回调函数是否为空，为空断言
   * 报错；不为空取出，从队列删除该元素
   * c.如果没有一个可执行对象是当前跑的线程应该执行的，就设置一个bool标志is_work表明当前
   * 是否有任务可做，没有转去执行idle()函数
   *
   * 2.开始执行从队列中拿出的可执行对象，分为：协程和函数。（本质都是要调度协程，只是兼容传入
   * 的可执行对象是函数的情况）
   * a. 如果为协程，并且当前线程应该工作is_work = true，分情况讨论：
   * ⅰ.协程处于没有执行完毕TERM且没有异常EXCEPTION，就swapIn()调入（继续）执行。调回（不
   * 一定是执行完毕了，也有可能到时了）后，如果处于就绪状态READY需要再一次加入队列中；
   * ⅱ.否则，调回后仍处于没有执行完毕TERM且没有异常EXCEPTION就要将其置为挂起HOLD状态。
   * b.如果为函数，整个流程和协程一模一样，只是需要使用一个指针创建一个协程使其能够被调度。
  }*/
  while (true) {
    /* 一、从消息队列取出可执行对象 */
    // 清空可执行对象
    ft.reset();
    // 是一个信号 没轮到当前线程执行任务 就要发出信号通知下一个线程去处理
    bool tickle_me = false;
    bool is_active = false;
    // 加锁
    {
      // 取出 待执行(即将要执行或者计划要执行)的协程队列m_fibers 的元素
      MutexType::Lock lock(m_mutex);
      auto it = m_fibers.begin();
      while (it != m_fibers.end()) {
        // a.当前任务没有指定线程执行 且 不是我当前线程要处理的协程
        // 跳过，不需要处理
        if (it->thread != -1 && it->thread != bin::GetThreadId()) {
          ++it;
          tickle_me = true;
          continue;
        }
        BIN_ASSERT(it->fiber || it->cb); // fiber 和 回调函数至少有一个
        // fiber正在执行状态，也不需要处理
        if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
          ++it;
          continue;
        }
        // b.是我当前线程要处理的任务/协程 就取出并且删除
        ft = *it;
        m_fibers.erase(it++);
        ++m_activeThreadCount;
        is_active = true;
        break;
      }
      tickle_me |= it != m_fibers.end(); // tickle_me |= (it != m_fibers.end());
    }
    if (tickle_me)
      tickle();
    /*二、根据可执行对象的类型 分为 协程和函数 来分别执行对应可执行操作*/
    // a. 如果要执行的任务是协程
    // 契合当前线程的协程还没执行完毕
    if (ft.fiber && (ft.fiber->getState() != Fiber::TERM &&
                     ft.fiber->getState() != Fiber::EXCEPT)) {
      BIN_LOG_INFO(g_logger) << "待执行对象: fiber, id = " << ft.fiber->getId();
      ft.fiber->swapIn(); // 让它执行，执行完做处理
      --m_activeThreadCount;
      // 从上面语句调回之后的处理 分为 还需要继续执行 和 需要挂起
      if (ft.fiber->getState() == Fiber::READY) {
        schedule(ft.fiber);
      } else if (ft.fiber->getState() != Fiber::TERM &&
                 ft.fiber->getState() != Fiber::EXCEPT) {
        ft.fiber->m_state = Fiber::HOLD; // 协程状态置为HOLD
      }
      // 可执行对象置空
      ft.reset();
    }
    // b. 如果要执行的任务是函数
    else if (ft.cb) {
      BIN_LOG_INFO(g_logger) << "待执行对象: cb";
      if (cb_fiber) { // 协程体的指针不为空就继续利用现有空间
        cb_fiber->reset(
            ft.cb); // power:
                    // 执行Fiber的reset()函数，上下文切换。重置协程函数，并重置状态
      } else {      // 为空就重新开辟
        cb_fiber.reset(new Fiber(ft.cb)); // 智能指针的reset()函数
      }
      ft.reset(); // FiberAndThread的reset函数 可执行对象置空
      cb_fiber->swapIn();
      --m_activeThreadCount;
      // 从上面语句调回之后的处理 分为 还需要继续执行 和 需要挂起
      if (cb_fiber->getState() == Fiber::READY) {
        schedule(cb_fiber);
        cb_fiber.reset(); // 智能指针置空
      } else if (cb_fiber->getState() == Fiber::EXCEPT ||
                 cb_fiber->getState() == Fiber::TERM) {
        cb_fiber->reset(nullptr); // 把执行任务置为空
      } else {
        cb_fiber->m_state = Fiber::HOLD; // 状态置为 HOLD
        cb_fiber.reset();                // 智能指针置空
      }
    } else { // c.没有任务需要执行  去执行idle() --->代表空转 然后设置为hold状态
      // BIN_LOG_INFO(g_logger)
      //     << "无任务执行，空转: idle() " << is_active << " "
      //     << m_activeThreadCount << " " << idle_fiber->getState();
      if (is_active) {
        --m_activeThreadCount;
        continue;
      }
      // 负责idle()的协程结束了 说明当前线程也结束了直接break
      if (idle_fiber->getState() == Fiber::TERM) {
        BIN_LOG_INFO(g_logger) << "idle fiber term";
        break;
      }
      ++m_idleThreadCount;
      idle_fiber->swapIn();
      --m_idleThreadCount;
      if (idle_fiber->getState() != Fiber::TERM &&
          idle_fiber->getState() != Fiber::EXCEPT) {
        idle_fiber->m_state = Fiber::HOLD; // 状态置为 HOLD
        // idle_fiber->m_state = Fiber::READY; //状态置为 HOLD
      }
    }
  }
}

/*
 * 几处用到tickle()地方：
 * ● schedule()添加任务，判断队列在本次添加前是否空：空就tickle()；不空不唤醒。
 * ● run()在轮询任务队列的时候，发现有任务不属于自己执行，并且也没有指定任何线程
 *   执行的时候要调用tickle()去唤醒线程执行这些任务
 * ● stop()准备关闭调度器时候，要把所有创建的线程唤醒，去让他们退出
 */
void Scheduler::tickle() { BIN_LOG_INFO(g_logger) << "tickle"; }

bool Scheduler::stopping() {
  MutexType::Lock lock(m_mutex);
  return m_autoStop && m_stopping && m_fibers.empty() &&
         m_activeThreadCount == 0;
}

void Scheduler::idle() {
  BIN_LOG_INFO(g_logger) << "idle";
  while (!stopping()) {
    bin::Fiber::YieldToHold();
  }
}

void Scheduler::switchTo(int thread) {
  BIN_ASSERT(Scheduler::GetThis() != nullptr);
  if (Scheduler::GetThis() == this) {
    if (thread == -1 || thread == bin::GetThreadId()) {
      return;
    }
  }
  schedule(Fiber::GetThis(), thread);
  Fiber::YieldToHold();
}

std::ostream &Scheduler::dump(std::ostream &os) {
  os << "[Scheduler name=" << m_name << " size=" << m_threadCount
     << " active_count=" << m_activeThreadCount
     << " idle_count=" << m_idleThreadCount << " stopping=" << m_stopping
     << " ]" << std::endl
     << "    ";
  for (size_t i = 0; i < m_threadIds.size(); ++i) {
    if (i) {
      os << ", ";
    }
    os << m_threadIds[i];
  }
  return os;
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler *target) {
  m_caller = Scheduler::GetThis();
  if (target) {
    target->switchTo();
  }
}

SchedulerSwitcher::~SchedulerSwitcher() {
  if (m_caller) {
    m_caller->switchTo();
  }
}

} // namespace bin

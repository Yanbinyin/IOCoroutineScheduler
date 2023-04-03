/**
 * @file coroutine.h
 * @author yinyb (990900296@qq.com)
 * @brief 协程封装类
 * @version 1.0
 * @date 2022-04-03
 * @copyright Copyright (c) {2022}
 */

/*
 * 概念：协程是用户态的线程，仅仅完成代码序列的切换而不陷入内核态发生内核资源的切换。
 *
 * 基本原理：采用非对称协程开发。所有子协程的调度和创建依赖于一个母协程init_coroutine，
 *  即：一个线程需要启动协程时，首先会创建出一个默认的母协程。
 *  通过母协程让出当前代码序列在CPU的执行权，转让到子协程对应的代码序列中继续执行，子协程完毕后
 *  让出执行权又回到母协程中。
 */

/*
 * 准备：依赖的库文件<ucontext.h>
 * ● 切换程序上下文API:
 * //依赖实现的结构体
 * typedef struct ucontext {
 *     struct ucontext *uc_link;		//后续的程序上下文
 *     sigset_t         uc_sigmask;	//当前上下文中的阻塞信号集合
 *     stack_t          uc_stack;		//当前上下文依赖的栈空间信息集合
 *     mcontext_t       uc_mcontext;
 *     ...
 * } ucontext_t;
 *
 * ● int getcontext(ucontext_t *ucp); 保存当前程序上下文。
 * ● int setcontext(const ucontext_t *ucp); 将保存的程序上下文拿出推送到CPU上
 * ● void makecontext(ucontext_t *ucp, void(*func)(), int argc, ...);
 * 创建新上下文 ● int swapcontext(ucontext_t *oucp,context_t *ucp);
 * 切换当前上下文 oucp---->ucp
 *
 * setcontext/swapcontext会激活保存好的程序上下文去执行，也就意味着必须先调用getcontext
 *
 * 注意：使用makecontext时，必须先调用一次getcontext（个人猜测是因为，重新创建一个上下文
 *  使用除了要传函数的入口地址，其余的CPU寄存器值也要保存，getcontext可以代劳），
 * 然后为ucontext_t设置新的栈空间传入：
 *  uc_stack.ss_sp栈起始地址，
 *  uc_stack.ss_size栈空间大小、
 *  后续需要运行的上下文uc_link
 */

/*
 * bar是一个智能指针，p是一个普通指针:
 * p = bar.get();
 *  bar并非被释放，也就相当于指针p和智能指针bar共同管理一个对象，所以就p做的一切，都会反应到
 *  bar指向的对象上
 *
 * p.reset(q)
 *  q为智能指针指向的对象，这样会使智能指针p指向q的空间，而且释放原来的空间(默认是delete)
 */

#ifndef __BIN_FIBER_H__
#define __BIN_FIBER_H__

#include <functional>
#include <memory>
#include <ucontext.h>

namespace bin {

/**
 * @brief 协程栈内存分配器类
 */
class MallocStackAllocator {
public:
  /**
   * @brief malloc memory
   * @param size meomory size
   */
  static void *Alloc(size_t size) {
    // malloc memory
    return malloc(size);
  }

  /**
   * @brief free meomroy
   * @param vp stack pointer
   * @param size stack size
   */
  static void Dealloc(void *vp, size_t size) {
    // size is for mmap
    return free(vp);
  }
};

class Scheduler;

/**
 * @brief 协程类
 */
class Fiber : public std::enable_shared_from_this<Fiber> {
  friend class Scheduler;

public:
  typedef std::shared_ptr<Fiber> ptr;

  // 协程状态
  enum State {
    INIT,  // 初始化状态
    HOLD,  // 暂停状态
    EXEC,  // 执行中状态
    TERM,  // 结束状态
    READY, // 可执行状态
    EXCEPT // 异常状态
  };

private:
  Fiber(); // 为init协程的生成而准备，构造每个线程下的第一个协程

public:
  /**
   * @brief Construct a new Fiber object
   * @param cb 协程执行的函数
   * @param stacksize 协程栈大小
   * @param use_caller 是否在MainFiber上调度
   */
  Fiber(std::function<void()> cb, size_t stacksize = 0,
        bool use_caller = false);
  ~Fiber();

  /**
   * @brief 重置协程执行函数,并设置状态
   *  getState()=INIT。协程执行完了，但是分配的内存没释放，基于这段内存创建新的协程
   * @param cb callback function
   */
  void reset(std::function<void()> cb);
  void swapIn();  // 将当前协程切换到运行状态，getState() = EXEC
  void swapOut(); // 将当前协程切换到后台
  void call(); // 将当前线程切换到执行状态，执行的为当前线程的主协程
  void back(); // 将当前线程切换到后台 pre:执行的为该协程 post:返回线程的主协程

  uint64_t getId() const { return m_id; }    // 返回协程id
  State getState() const { return m_state; } // 返回协程状态

public:
  // 设置当前线程的运行协程    f：运行协程
  static void SetThis(Fiber *f);
  // 返回当前所在的协程，如果当前线程没有协程，会初始化一个主协程
  static Fiber::ptr GetThis();

  // 将当前协程切换到后台,并设置为READY状态  getState() = READY
  static void YieldToReady();
  // 将当前协程切换到后台,并设置为HOLD状态   getState() = HOLD
  static void YieldToHold();

  // 返回当前协程的总数量
  static uint64_t TotalFibers();
  // 获取当前协程的id
  static uint64_t GetFiberId();

  // 协程执行函数，执行完成返回到线程主协程
  static void MainFunc();
  // 协程执行函数，执行完成返回到线程调度协程
  static void CallerMainFunc();

private:
  uint64_t m_id = 0;          // 协程id
  void *m_stack = nullptr;    // 协程运行栈指针
  uint32_t m_stacksize = 0;   // 协程运行栈大小
  State m_state = INIT;       // 协程状态
  ucontext_t m_ctx;           // 协程上下文
  std::function<void()> m_cb; // 协程运行函数
};

} // namespace bin

#endif

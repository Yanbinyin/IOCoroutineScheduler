/**
 * @file iomanager.h
 * @author yinyb (990900296@qq.com)
 * @brief 基于Epoll的IO协程调度器
 *  封装套接字句柄对象+事件对象,方便归纳句柄、事件所具有的属性。句柄带有事件，事件依附
 *  于句柄。只有句柄上才有事件触发。
 *  和HOOK模块的文件句柄类做一个区分，这里的套接字句柄专门针对套接字上的事件做回调管理。
 * @version 1.0
 * @date 2022-04-02
 * @copyright Copyright (c) {2022}
 */

#ifndef __BIN_IOMANAGER_H__
#define __BIN_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace bin {

// 基于Epoll的IO协程调度器
class IOManager : public Scheduler, public TimerManager {
public:
  typedef std::shared_ptr<IOManager> ptr;
  typedef RWMutex RWMutexType;

  /**
   * @brief IO event
   */
  enum Event {
    NONE = 0x0,  // 无事件
    READ = 0x1,  // 读事件(EPOLLIN)
    WRITE = 0x4, // 写事件(EPOLLOUT)
  };

  /**
   * @brief 句柄对象结构体, Socket事件上线文类
   */
  struct FdContext {
    typedef Mutex MutexType;

    /**
     * @brief 事件对象结构体,事件上线文类
     */
    struct EventContext {
      // 执行事件的调度器。支持多线程、多协程、多个协程调度器，指定which
      Scheduler *scheduler = nullptr;
      Fiber::ptr fiber;         // 事件绑定的协程
      std::function<void()> cb; // 事件的绑定回调函数
    };

    // 根据event类型获取句柄对象上对应的事件对象
    EventContext &getContext(Event event);
    // 清空句柄对象(ctx)上的事件对象
    void resetContext(EventContext &ctx);
    // 主动触发事件(event)，执行事件对象上的回调函数
    void triggerEvent(Event event);

    int fd = 0;          // 事件关联的句柄//句柄/文件描述符
    Event events = NONE; // 当前的事件//句柄上注册好的事件
    EventContext read;   // 读事件上下文//句柄上的读事件对象
    EventContext write;  // 写事件上下文//句柄上的写事件对象
    MutexType mutex;     // 事件的Mutex
  };

public:
  /**
   * @brief Construct a new IOManager object
   * @param threads_size 线程数量
   * @param use_caller 是否将调用线程包含进去
   * @param name 调度器的名称
   */
  IOManager(size_t threads_size = 1, bool use_caller = true,
            const std::string &name = "");
  ~IOManager();

  /**
   * @brief 添加事件
   * @param fd socket句柄，给fd这个句柄添加事件
   * @param event 事件类型，读/写事件
   * @param cb callback function
   * @return 0成功,-1失败
   */
  int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
  /**
   * @brief 删除事件，不会触发事件
   * @param fd socket句柄，给fd这个句柄删除事件
   * @param event 事件类型，读/写事件
   * @return return success or not
   */
  bool delEvent(int fd, Event event);
  /**
   * @brief 取消事件，如果事件存在则触发事件
   * @param fd socket句柄，给fd这个句柄取消事件
   * @param event 事件类型，读/写事件
   * @return return success or not
   */
  bool cancelEvent(int fd, Event event);
  /**
   * @brief 取消fd句柄的所有事件  
   * @param fd socket句柄
   * @return return success or not
   */
  bool cancelAll(int fd);

  /**
   * @brief 返回当前的IOManager
   * @return IOManager*
   */
  static IOManager *GetThis();

protected:
  void tickle() override;
  bool stopping() override;
  void idle() override;

  void onTimerInsertedAtFront() override;

  /**
   * @brief 重置socket句柄上下文的容器(m_fdContexts)大小
   * @param size 要设置的大小
   */
  void contextResize(size_t size);
  /**
   * @brief 判断是否可以停止
   * @param timeout 最近要出发的定时器事件间隔
   * @return success or not
   */
  bool stopping(uint64_t &timeout);

private:
  int m_epfd = 0;                                // epoll 文件句柄
  int m_tickleFds[2];                            // pipe 文件句柄
  std::atomic<size_t> m_pendingEventCount = {0}; // 当前等待执行的事件数量
  std::vector<FdContext *> m_fdContexts; // socket事件上下文的容器
  RWMutexType m_mutex;
};

} // namespace bin

#endif

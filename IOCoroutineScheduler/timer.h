/**
 * @file timer.h
 * @author yinyb (990900296@qq.com)
 * @brief 定时器
 *  在间隔一个指定时间后，会自动的往任务队列中添加一个函数/协程，
 *  执行我们需要的操作。支持一次性定时器触发和循环定时器触发
 *  使用场景：在特定时间段或者某一段时间内需要执行一些任务等场景
 * @version 1.0
 * @date 2022-04-01
 * @copyright Copyright (c) {2022}
 */

#ifndef __BIN_TIMER_H__
#define __BIN_TIMER_H__

#include <memory>
#include <set>
#include <vector>

#include "thread.h"

namespace bin {

class TimerManager;

/**
 * @brief Timer类，构造函数私有属性,不能直接创建其对象
 *  将其对象创建交由TimerManager类负责管理
 *  Timer和TimerManager互相置为友元类
 *  方便TimerManager的addTimer()调用Timer的private constructor
 */
class Timer : public std::enable_shared_from_this<Timer> {
  friend class TimerManager;

public:
  typedef std::shared_ptr<Timer> ptr;
  /**
   * @brief 取消当前定时器的定时任务
   * @return success or not
   */
  bool cancel();
  /**
   * @brief 刷新设置定时器的执行时间
   * @return success or not
   */
  bool refresh();
  /**
   * @brief 重置定时器时间
   * @param ms 定时器执行间隔时间(毫秒);
   * @param from_now 是否从当前时间开始计算
   * @return true 存在并更改成功
   * @return false 不存在或更改失败
   */
  bool reset(uint64_t ms, bool from_now);

private:
  /**
   * @brief Construct a new Timer object
   *  Timer的构造函数为私有意味着不能显示创建对象，必须由TimerManager来创建
   * @param ms 定时器执行间隔时间
   * @param cb callback function
   * @param recurring 是否循环执行
   * @param manager TimerManager
   */
  Timer(uint64_t ms, std::function<void()> cb, bool recurring,
        TimerManager *manager);
  /**
   * @brief Construct a new Timer object
   * @param next 执行的时间戳(毫秒)
   */
  Timer(uint64_t next);

private:
  bool m_recurring = false;          // 是否循环定时器
  uint64_t m_ms = 0;                 // 执行周期
  uint64_t m_next = 0;               // 精确的执行时间
  std::function<void()> m_cb;        // 回调函数
  TimerManager *m_manager = nullptr; // 定时器管理器

private:
  /**
   * @brief 定时器比较仿函数
   *  Timer内部构建仿函数作为set容器比较的函数调用，set存储指针默认通过指针值排列
   *  我们希望通过比较到期时间来进行排列
   */
  struct Comparator {
    // 比较定时器的智能指针的大小(按执行时间排序)
    bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
  };
};

/**
 * @brief 定时器管理器
 *  虚基类，管理Timer定时器队列，让其能够有序的执行到期后的定时任务
 *  让其继承类通过纯虚函数 onTimerInsertedAtFront() 去加入相关的其他业务拓展
 */
class TimerManager {
  friend class Timer;

public:
  typedef RWMutex RWMutexType;

  TimerManager();
  virtual ~TimerManager();

  /**
   * @brief 创建定时器并添加到定时器集合(m_timers)中
   * @param ms 定时器执行间隔时间
   * @param cb callback function
   * @param recurring 是否循环定时器
   * @return 创建的定时器
   */
  Timer::ptr addTimer(uint64_t ms, std::function<void()> cb,
                      bool recurring = false);
  /**
   * @brief 创建定时器并添加到定时器集合(m_timers)中
   * @param ms 定时器执行间隔时间
   * @param cb callback function
   * @param weak_cond 条件
   * @param recurring 是否循环定时器
   * @return 创建的定时器
   */
  Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb,
                               std::weak_ptr<void> weak_cond,
                               bool recurring = false);
  /**
   * @brief 获取需要执行的定时器的回调函数列表
   * @param cbs 包含cb的回调函数数组
   */
  void listExpiredCb(std::vector<std::function<void()>> &cbs);
  /**
   * @brief 获取当前定时器队列队头的到期时间点的值,如果超出了触发时间点，返回0
   * @return the Next Timer object 
   */
  uint64_t getNextTimer(); // 获取到最近一个定时器执行的时间间隔(毫秒)
  /**
   * @brief 是否有定时器
   */
  bool hasTimer();

protected:
  /**
   * @brief 当有新的定时器插入到定时器的首部,执行该函数
   */
  virtual void onTimerInsertedAtFront() = 0;
  /**
   * @brief 将定时器添加到管理器中
   * @param val 定时器
   * @param lock 锁
   */
  void addTimer(Timer::ptr val, RWMutexType::WriteLock &lock);

private:
  /**
   * @brief 检测服务器时间是否被调后了
   *  重点检查时间被调小的情况因为可能会导致定时器队列里所有任务都触发。
   * @param now_ms 当前时间
   * @return true 被调后了
   * @return false 未被调后
   */
  bool detectClockRollover(uint64_t now_ms);

private:
  RWMutexType m_mutex;
  std::set<Timer::ptr, Timer::Comparator> m_timers; // 定时器集合
  // 是否触发onTimerInsertedAtFront，避免频繁修改的一个ticked标记
  bool m_tickled = false;
  uint64_t m_previouseTime = 0; // 上次执行时间
};

} // namespace bin

#endif

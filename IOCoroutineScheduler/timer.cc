/**
 * @file timer.cc
 * @author yinyb (990900296@qq.com)
 * @brief
 * @version 1.0
 * @date 2022-04-01
 * @copyright Copyright (c) {2022}
 */

#include "timer.h"
#include "util.h"

namespace bin {

bool Timer::Comparator::operator()(const Timer::ptr &lhs,
                                   const Timer::ptr &rhs) const {
  // 通过地址判断是否有值
  if (!lhs && !rhs) {
    return false;
  }
  if (!lhs) {
    return true;
  }
  if (!rhs) {
    return false;
  }

  // 都有值 判断到期时间
  if (lhs->m_next == rhs->m_next) {
    // 到期时间相等的话 地址小的优先调度
    return lhs.get() < rhs.get();
  } else {
    // 不相等，到期时间小的优先调度
    return lhs->m_next < rhs->m_next;
  }
}

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring,
             TimerManager *manager)
    : m_recurring(recurring), m_ms(ms), m_cb(cb), m_manager(manager) {
  m_next = bin::GetCurrentMS() + m_ms; // 计算到期时间点
}

Timer::Timer(uint64_t next) : m_next(next) {}

bool Timer::cancel() {
  // 通过操作TimerManger的定时器队列，通过比较定时器的内存地址以及到期时间点
  // 找到对应的定时器对象，从定时器队列中移除。
  TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
  if (m_cb) {
    m_cb = nullptr;
    auto it = m_manager->m_timers.find(shared_from_this());
    m_manager->m_timers.erase(it);
    return true;
  }
  return false;
}

bool Timer::refresh() {
  // 重新刷新定时器的间隔时间，让其重新开始新的计时间隔，但不改变原来设定好的时间间隔量
  // set底层是红黑树，有序，直接原地修改会打乱有序规则，发生未知错误，必须将原来的定时
  // 器从这个有序序列中移除，再重新加入才能合法
  TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
  if (!m_cb) {
    return false;
  }
  auto it = m_manager->m_timers.find(shared_from_this());
  // 1、没找到
  if (it == m_manager->m_timers.end()) {
    return false;
  }
  // 2、找到了：由于使用的set 要先移除再进行重置加入
  m_manager->m_timers.erase(it);
  m_next = bin::GetCurrentMS() + m_ms;
  m_manager->m_timers.insert(shared_from_this());

  return true;
}

/*
功能：重新设定当前定时器的时间间隔量。可以选择即刻生效或者继续上一次旧的间隔量基础上等待触发。默认从此刻重新开始计时触发。
注意：重新设置时间间隔量，可能存在比原来的间隔量大，也可能比原来的间隔量小，都需要考虑
● 核心逻辑：
1.
判断重新设定的时间间隔是否和原来相等，且重新设定是否从即刻生效。（如果和原来相等，并且即刻生效，其功能就会变成refresh()。）
2.
从TimerManaer的定时器队列里找出对应的定时器移除，并且重新计算定时器的到期时间点：
    a. 如果from_now =
true，即：即刻生效。获取当前时间GetCurrentMs()，将其作为新的时间起点，然后又加上新的时间间隔量。
    b. 如果from_now =
false，即：不需要即刻生效。计算得到原来的时间起点原时间起点 = 原触发时间点 -
原时间间隔，在原时间起点的基础上加上新的时间间隔来完成触发。
        也就是说这种重置定时器的做法，设置完之后的定时器已经过了一部分时间，计划等待时间
>= 实际等待时间
3.
调用TimerManager::addTimer()将定时器重新加入队列，不直接使用set.insert()的原因在于：
当前定时器的到期时间点 <
队头定时器(队列到期时间点最近的定时器)到期时间点，在加入队列时存在这种可能，需要tickle()去唤醒epoll_wait去修改其超时时间。
*/
bool Timer::reset(uint64_t ms, bool from_now) {
  if (ms == m_ms &&
      !from_now) { // 新时间周期等于现时间周期 并且 起始时间不是现在，无需更改
    return true;
  }
  TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
  if (!m_cb) { // 没有任务也无需修改直接返回
    return false;
  }
  auto it = m_manager->m_timers.find(shared_from_this());

  // 1、管理器内没找到
  if (it == m_manager->m_timers.end()) {
    return false;
  }
  // 2、管理器内找到了
  m_manager->m_timers.erase(it); // 注意：由于使用的set 要先移除再进行重置加入
  uint64_t start = 0;
  if (from_now) { // 重新从现在开始计时
    start = bin::GetCurrentMS();
  } else { // 继续上一次的计时
    start = m_next - m_ms;
  }
  m_ms = ms;             // 设置新的计时间隔
  m_next = start + m_ms; // 设置新的触发时间点
  // 用addTimer不用reset是因为reset可能出现执行时间变成最小的可能(放在队头)
  // 会有一次唤醒
  m_manager->addTimer(shared_from_this(), lock);

  return true;
}

TimerManager::TimerManager() { m_previouseTime = bin::GetCurrentMS(); }

TimerManager::~TimerManager() {}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb,
                                  bool recurring) {
  // 在TimerManager 中构造Timer
  Timer::ptr timer(new Timer(ms, cb, recurring, this));
  RWMutexType::WriteLock lock(m_mutex);
  addTimer(timer, lock);
  return timer;
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
  std::shared_ptr<void> tmp = weak_cond.lock();
  // 利用弱指针weak_ptr 来判断条件是否存在
  if (tmp) {
    cb(); // execute the callback function.
  }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms,
                                           std::function<void()> cb,
                                           std::weak_ptr<void> weak_cond,
                                           bool recurring) {
  // 添加一个条件定时器，和普通定时器相比，多了一个用weak_ptr<>弱指针指向shared_ptr<>
  // 共享指针管理的"条件"，定时器触发时检查"条件"是否还存在，不存在将不执行所持有的任务
  return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
}

uint64_t TimerManager::getNextTimer() {
  RWMutexType::ReadLock lock(m_mutex);
  // 获取队头定时器时间一次 就可以去唤醒一次
  m_tickled = false;

  // 定时器队列为空返回一个极大值
  if (m_timers.empty())
    return ~0ull;

  const Timer::ptr &next = *m_timers.begin();
  uint64_t now_ms = bin::GetCurrentMS();
  if (now_ms >=
      next->m_next) // 现在获取的时间 已经晚于预计要触发的时间点 马上执行
    return 0;
  else // 还没到预定时间就返回剩余时间间隔
    return next->m_next - now_ms;
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs) {
  // 检查当前的定时队列，找出所有已经到时需要触发的定时器，从定时器队列中移除，并且将这
  // 些已经要触发的定时器中的函数/协程全部取出放入一个目标容器内。
  // 注意：处理服务器时间被修改的情况，会对定时器触发有一定的影响
  uint64_t now_ms = bin::GetCurrentMS();
  std::vector<Timer::ptr> expired;
  {
    RWMutexType::ReadLock lock(m_mutex);
    if (m_timers.empty())
      return;
  }

  // 先读锁，再写锁，检查了两次empty()
  RWMutexType::WriteLock lock(m_mutex);
  if (m_timers.empty()) {
    return;
  }
  bool rollover = detectClockRollover(now_ms);

  // 服务器时间没有发生改变 且不存在超时定时器 就退出函数
  if (!rollover && ((*m_timers.begin())->m_next > now_ms))
    return;

  // 构造一个装有当前时间的Timer为了应用lower_bound算法函数
  Timer::ptr now_timer(new Timer(now_ms));

  // 如果服务器时间发生了变动 就返回end() 会将整个m_timers全部清理 重新加入队列
  // 否则 将找出大于等于当前时间的第一个Timer
  auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);

  // 找到还没到时的第一个定时器的位置后 停下
  while (it != m_timers.end() && (*it)->m_next == now_ms)
    ++it;

  // 将队头~当前位置之前 所有已经到时的定时器全部拿出 到expired队列中
  expired.insert(expired.begin(), m_timers.begin(), it);
  // 将原队列前部到期的定时器全部移除
  m_timers.erase(m_timers.begin(), it);

  // 扩充可执行任务队列空间,
  // reserve()强制容器把它的容量改为至少n，提供的n不小于当前大小
  cbs.reserve(expired.size());

  for (auto &tmr : expired) { // tmr : timer
    cbs.push_back(tmr->m_cb);
    // 循环定时器就放回 set中
    if (tmr->m_recurring) {
      tmr->m_next = now_ms + tmr->m_ms;
      m_timers.insert(tmr);
    } else
      tmr->m_cb = nullptr;
  }
}

void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock &lock) {
  auto it = m_timers.insert(val).first;
  bool at_front = (it == m_timers.begin()) && !m_tickled;
  // 频繁修改时候 避免总是去唤醒
  if (at_front) {
    m_tickled = true;
  }
  lock.unlock(); // 解锁重载函数addTimer()中添加的锁

  // 当有比之前更小的
  // 定时器任务插入，需要去唤醒线程修改对应的epoll_wait的超时时间
  // epoll_wait阻塞的线程可能10s后唤醒，但是这个3s后唤醒，不能等待epoll_wait了，用
  // onTimerInsertAtFront()不然会超时
  // power: 实际上：将挂起的协程唤醒，重新走一遍流程idle()---->run()--->idle()
  if (at_front) {
    onTimerInsertedAtFront();
  }
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
  // now_ms < 上一次记录的时间 && now_ms < 上一次记录的时间的一个小时前的时间
  bool rollover = false;
  if (now_ms < m_previouseTime && now_ms < (m_previouseTime - 60 * 60 * 1000)) {
    rollover = true;
  }
  m_previouseTime = now_ms;
  return rollover;
}

bool TimerManager::hasTimer() {
  RWMutexType::ReadLock lock(m_mutex);
  return !m_timers.empty();
}

} // namespace bin

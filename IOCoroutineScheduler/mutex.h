/**
 * @file mutex.h
 * @author yinyb (990900296@qq.com)
 * @brief 索封装类
 * @version 1.0
 * @date 2022-04-03
 * @copyright Copyright (c) {2022}
 */

#ifndef __BIN_MUTEX_H__
#define __BIN_MUTEX_H__

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <thread>

#include "coroutine.h"
#include "noncopyable.h"
/*
    C++没有互斥锁以及读写分离的锁，因此选择自己封装
*/
namespace bin {

// block:信号量
class Semaphore : Noncopyable {
public:
  Semaphore(uint32_t count = 0); // 构造函数  count 信号量值的大小
  ~Semaphore();

  void wait();   // 获取信号量，数-1
  void notify(); // 释放信号量，数+1

private:
  sem_t m_semaphore;
};

// block: 互斥锁
// 局部区域互斥锁模板类
// T：锁的类型
// 互斥量一般都是在一个局部范围
// 为了防止漏掉解锁，一般写一个类，通过构造函数加锁，析构函数解锁
template <class T> struct ScopedLockImpl {
public:
  ScopedLockImpl(T &mutex) : m_mutex(mutex) {
    m_mutex.lock(); // 构造时加锁
    m_locked = true;
  }

  ~ScopedLockImpl() {
    unlock(); // 自动释放锁
  }

  void lock() {
    if (!m_locked) {
      m_mutex.lock();
      m_locked = true;
    }
  }

  void unlock() {
    if (m_locked) {
      m_mutex.unlock();
      m_locked = false;
    }
  }

private:
  T &m_mutex;    // mutex
  bool m_locked; // 是否已上锁
};

// 互斥锁类
class Mutex : Noncopyable {
public:
  typedef ScopedLockImpl<Mutex> Lock; // 局部锁

  Mutex() { pthread_mutex_init(&m_mutex, nullptr); }

  ~Mutex() { pthread_mutex_destroy(&m_mutex); }

  void lock() { pthread_mutex_lock(&m_mutex); }

  void unlock() { pthread_mutex_unlock(&m_mutex); }

private:
  pthread_mutex_t m_mutex;
};

// block: 读写锁
// 局部读锁模板实现
template <class T> struct ReadScopedLockImpl {
public:
  ReadScopedLockImpl(T &mutex) : m_mutex(mutex) {
    m_mutex.rdlock();
    m_locked = true;
  }

  ~ReadScopedLockImpl() {
    unlock(); // 自动释放锁
  }

  void lock() {
    if (!m_locked) {
      m_mutex.rdlock();
      m_locked = true;
    }
  }

  void unlock() {
    if (m_locked) {
      m_mutex.unlock();
      m_locked = false;
    }
  }

private:
  T &m_mutex;    // mutex
  bool m_locked; // 是否已上锁
};

// 局部写锁模板实现
template <class T> struct WriteScopedLockImpl {
public:
  WriteScopedLockImpl(T &mutex) : m_mutex(mutex) {
    m_mutex.wrlock();
    m_locked = true;
  }

  ~WriteScopedLockImpl() { unlock(); }

  void lock() {
    if (!m_locked) {
      m_mutex.wrlock();
      m_locked = true;
    }
  }

  void unlock() {
    if (m_locked) {
      m_mutex.unlock();
      m_locked = false;
    }
  }

private:
  T &m_mutex;    // Mutex
  bool m_locked; // 是否已上锁
};

// 读写互斥量
class RWMutex : Noncopyable {
public:
  typedef ReadScopedLockImpl<RWMutex> ReadLock;   // 局部读锁
  typedef WriteScopedLockImpl<RWMutex> WriteLock; // 局部写锁

  RWMutex() { pthread_rwlock_init(&m_lock, nullptr); }

  ~RWMutex() { pthread_rwlock_destroy(&m_lock); }

  void rdlock() { pthread_rwlock_rdlock(&m_lock); }

  void wrlock() { pthread_rwlock_wrlock(&m_lock); }

  void unlock() { pthread_rwlock_unlock(&m_lock); }

private:
  pthread_rwlock_t m_lock; // 读写锁
};

// block: 自旋锁
class Spinlock : Noncopyable {
public:
  typedef ScopedLockImpl<Spinlock> Lock; // 局部锁

  Spinlock() { pthread_spin_init(&m_mutex, 0); }

  ~Spinlock() { pthread_spin_destroy(&m_mutex); }

  void lock() { pthread_spin_lock(&m_mutex); }

  void unlock() { pthread_spin_unlock(&m_mutex); }

private:
  // 自旋锁
  pthread_spinlock_t m_mutex;
};

// block: 原子锁
class CASLock : Noncopyable {
public:
  typedef ScopedLockImpl<CASLock> Lock; // 局部锁

  CASLock() { m_mutex.clear(); }

  ~CASLock() {}

  void lock() {
    // 执行本次原子操作之前 所有 读 原子操作必须全部完成
    while (std::atomic_flag_test_and_set_explicit(&m_mutex,
                                                  std::memory_order_acquire))
      ;
  }

  void unlock() {
    // 执行本次原子操作之前 所有 写 原子操作必须全部完成
    std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
  }

private:
  // 原子状态
  volatile std::atomic_flag m_mutex;
};

// block: 协程信号量
class Scheduler;
class FiberSemaphore : Noncopyable {
public:
  typedef Spinlock MutexType;

  FiberSemaphore(size_t initial_concurrency = 0);
  ~FiberSemaphore();

  bool tryWait();
  void wait();
  void notify();

  size_t getConcurrency() const { return m_concurrency; }
  void reset() { m_concurrency = 0; }

private:
  MutexType m_mutex;
  std::list<std::pair<Scheduler *, Fiber::ptr>> m_waiters;
  size_t m_concurrency;
};

// block: 调试用的空锁

// 空锁(用于调试)
class NullMutex : Noncopyable {
public:
  typedef ScopedLockImpl<NullMutex> Lock; // 局部锁

  NullMutex() {}
  ~NullMutex() {}

  void lock() {}   // 加锁
  void unlock() {} // 解锁
};

// 空读写锁(用于调试)
class NullRWMutex : Noncopyable {
public:
  typedef ReadScopedLockImpl<NullMutex> ReadLock;
  typedef WriteScopedLockImpl<NullMutex> WriteLock;

  NullRWMutex() {}
  ~NullRWMutex() {}

  void rdlock() {} // 上读锁
  void wrlock() {} // 上写锁
  void unlock() {} // 解锁
};

} // namespace bin

#endif

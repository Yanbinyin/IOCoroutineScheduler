/**
 * @file thread.h
 * @author yinyb (990900296@qq.com)
 * @brief 线程封装类
 * @version 1.0
 * @date 2022-04-03
 * @copyright Copyright (c) {2022}
 */

#ifndef __BIN_THREAD_H__
#define __BIN_THREAD_H__

#include "mutex.h"

namespace bin {

/**
 * @brief 线程类
 *  互斥量 信号量 禁止拷贝
 */
class Thread : Noncopyable {
public:
  typedef std::shared_ptr<Thread> ptr;

  /**
   * @brief Construct a new Thread object
   *  利用pthread库开启运行线程，并且置一个信号量去等待线程完全开启后再退出构造函数
   * @param cb the function that executes thread
   * @param name thread name
   */
  Thread(std::function<void()> cb, const std::string &name);

  /**
   * @brief Destroy the Thread object
   */
  ~Thread();

  /**
   * @brief get the thread ID
   */
  pid_t getId() const { return m_id; }

  /**
   * @brief get the m_thread
   */
  pthread_t getThread() const { return m_thread; }

  /**
   * @brief get the thread name
   */
  const std::string &getName() const { return m_name; }

  /**
   * @brief get current thread pointer
   */
  static Thread *GetThis();
  /**
   * @brief get the current thread name
   */
  static const std::string &GetName();

  /**
   * @brief set the current thread name
   */
  static void SetName(const std::string &name);

  /**
   * @brief 等待线程执行完成
   */
  void join();

private:
  /**
   * @brief 线程执行函数
   * @param arg fuction that be executed in thread
   */
  static void *run(void *arg);

private:
  /// 线程id，run()中初始化，用户态线程ID和内核线程ID不是一个概念，调试时候需要拿到内核中的ID
  pid_t m_id = -1;
  /// 线程结构(unsigned long), pthread_create()中初始化
  pthread_t m_thread = 0;
  /// the function that executes thread
  std::function<void()> m_cb;
  /// thread name
  std::string m_name;
  /// semaphore
  Semaphore m_semaphore;
};

} // namespace bin

#endif

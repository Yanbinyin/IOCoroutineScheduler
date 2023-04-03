/**
 * @file log.h
 * @author yinyb (990900296@qq.com)
 * @brief 日志封装
 * @version 1.0
 * @date 2022-04-02
 * @copyright Copyright (c) {2022}
 */

#ifndef _BIN_LOG_H_
#define _BIN_LOG_H_

#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <stdarg.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "singleton.h"
#include "thread.h"
#include "util.h"

/**
 * @brief 使用流式方式将日志级别level的日志写入到logger
 */
#define BIN_LOG_LEVEL(logger, level)                                           \
  if (logger->getLevel() <= level)                                             \
  bin::LogEventWrap(                                                           \
      bin::LogEvent::ptr(new bin::LogEvent(                                    \
          logger, level, __FILE__, __LINE__, 0, bin::GetThreadId(),            \
          bin::GetFiberId(), time(0), bin::Thread::GetName())))                \
      .getSS()

/**
 * @brief 使用流式方式将日志级别debug的日志写入到logger
 */
#define BIN_LOG_DEBUG(logger) BIN_LOG_LEVEL(logger, bin::LogLevel::DEBUG)

/**
 * @brief 使用流式方式将日志级别info的日志写入到logger
 */
#define BIN_LOG_INFO(logger) BIN_LOG_LEVEL(logger, bin::LogLevel::INFO)

/**
 * @brief 使用流式方式将日志级别warn的日志写入到logger
 */
#define BIN_LOG_WARN(logger) BIN_LOG_LEVEL(logger, bin::LogLevel::WARN)

/**
 * @brief 使用流式方式将日志级别error的日志写入到logger
 */
#define BIN_LOG_ERROR(logger) BIN_LOG_LEVEL(logger, bin::LogLevel::ERROR)

/**
 * @brief 使用流式方式将日志级别fatal的日志写入到logger
 */
#define BIN_LOG_FATAL(logger) BIN_LOG_LEVEL(logger, bin::LogLevel::FATAL)

/**
 * @brief 使用格式化方式将日志级别level的日志写入到logger
 */
#define BIN_LOG_FMT_LEVEL(logger, level, fmt, ...)                             \
  if (logger->getLevel() <= level)                                             \
  bin::LogEventWrap(                                                           \
      bin::LogEvent::ptr(new bin::LogEvent(                                    \
          logger, level, __FILE__, __LINE__, 0, bin::GetThreadId(),            \
          bin::GetFiberId(), time(0), bin::Thread::GetName())))                \
      .getEvent()                                                              \
      ->format(fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别debug的日志写入到logger
 */
#define BIN_LOG_FMT_DEBUG(logger, fmt, ...)                                    \
  BIN_LOG_FMT_LEVEL(logger, bin::LogLevel::DEBUG, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别info的日志写入到logger
 */
#define BIN_LOG_FMT_INFO(logger, fmt, ...)                                     \
  BIN_LOG_FMT_LEVEL(logger, bin::LogLevel::INFO, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别warn的日志写入到logger
 */
#define BIN_LOG_FMT_WARN(logger, fmt, ...)                                     \
  BIN_LOG_FMT_LEVEL(logger, bin::LogLevel::WARN, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别error的日志写入到logger
 */
#define BIN_LOG_FMT_ERROR(logger, fmt, ...)                                    \
  BIN_LOG_FMT_LEVEL(logger, bin::LogLevel::ERROR, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别fatal的日志写入到logger
 */
#define BIN_LOG_FMT_FATAL(logger, fmt, ...)                                    \
  BIN_LOG_FMT_LEVEL(logger, bin::LogLevel::FATAL, fmt, __VA_ARGS__)

/**
 * @brief 获取主日志器
 */
#define BIN_LOG_ROOT() bin::LoggerMgr::GetInstance()->getRoot()

/**
 * @brief 获取name的日志器
 *  返回一个唯一的singleton LogManager指针
 */
#define BIN_LOG_NAME(name) bin::LoggerMgr::GetInstance()->getLogger(name)

namespace bin {

class Logger;
class LoggerManager;

/**
 * @brief 日志级别
 */
class LogLevel {
public:
  /**
   * @brief 日志级别枚举
   */
  enum Level {
    UNKNOW = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
  };

  /**
   * @brief 将日志级别转成文本输出
   * @param level 日志级别
   */
  static const char *ToString(LogLevel::Level level);

  /**
   * @brief 将文本转换成日志级别
   * @param str 日志级别文本
   */
  static LogLevel::Level FromString(const std::string &str);
};

/**
 * @brief 日志事件
 */
class LogEvent {
public:
  typedef std::shared_ptr<LogEvent> ptr;
  /**
   * @brief 构造函数
   * @param logger 日志器
   * @param level 日志级别
   * @param file 文件名
   * @param line 文件行号
   * @param elapse 程序启动依赖的耗时(毫秒)
   * @param thread_id 线程id
   * @param fiber_id 协程id
   * @param time 日志事件(秒)
   * @param thread_name 线程名称
   */
  LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
           const char *file, int32_t line, uint32_t elapse, uint32_t thread_id,
           uint32_t fiber_id, uint64_t time, const std::string &thread_name);

  /**
   * @brief 返回文件名
   */
  const char *getFile() const { return m_file; }

  /**
   * @brief 返回行号
   */
  int32_t getLine() const { return m_line; }

  /**
   * @brief 返回耗时
   */
  uint32_t getElapse() const { return m_elapse; }

  /**
   * @brief 返回线程ID
   */
  uint32_t getThreadId() const { return m_threadId; }

  /**
   * @brief 返回协程ID
   */
  uint32_t getFiberId() const { return m_fiberId; }

  /**
   * @brief 返回时间
   */
  uint64_t getTime() const { return m_time; }

  /**
   * @brief 返回线程名称
   */
  const std::string &getThreadName() const { return m_threadName; }

  /**
   * @brief 返回日志内容
   */
  std::string getContent() const { return m_ss.str(); }

  /**
   * @brief 返回日志器
   */
  std::shared_ptr<Logger> getLogger() const { return m_logger; }

  /**
   * @brief 返回日志级别
   */
  LogLevel::Level getLevel() const { return m_level; }

  /**
   * @brief 返回日志内容字符串流
   */
  std::stringstream &getSS() { return m_ss; }

  /**
   * @brief 格式化写入日志内容
   */
  void format(const char *fmt, ...);

  /**
   * @brief 格式化写入日志内容
   */
  void format(const char *fmt, va_list al);

private:
  /// 文件名
  const char *m_file = nullptr;
  /// 行号
  int32_t m_line = 0;
  /// 程序启动开始到现在的毫秒数
  uint32_t m_elapse = 0;
  /// 线程ID
  uint32_t m_threadId = 0;
  /// 协程ID
  uint32_t m_fiberId = 0;
  /// 时间戳
  uint64_t m_time = 0;
  /// 线程名称
  std::string m_threadName;
  /// 日志内容流
  std::stringstream m_ss;
  /// 日志器
  std::shared_ptr<Logger> m_logger;
  /// 日志等级
  LogLevel::Level m_level;
};

/**
 * @brief 日志事件包装器
 */
/*
  目的：//hack:为什么直接封装LogEvent不行
      封装便捷的宏函数调用时，无法使得LogEvent对象正常析构。
      封装一层包装器，在其析构函数中去实现原本想实现的功能，而又不产生内存泄漏。
      使用了一个LogEventWrap的匿名对象，由于匿名对象的生命周期只有一行，因此会马上回收空间触发析构函数。
      析构函数中就有对应日志打印语句:Logger::log()。
*/
class LogEventWrap {
public:
  /**
   * @brief 构造函数
   * @param e 日志事件
   */
  LogEventWrap(LogEvent::ptr e);

  /**
   * @brief 析构函数
   */
  ~LogEventWrap();

  /**
   * @brief 获取日志事件
   */
  LogEvent::ptr getEvent() const { return m_event; }

  /**
   * @brief 获取日志内容流
   */
  std::stringstream &getSS();

private:
  /**
   * @brief 日志事件
   */
  LogEvent::ptr m_event;
};

/**
 * @brief 日志格式化
 */
class LogFormatter {
public:
  typedef std::shared_ptr<LogFormatter> ptr;
  /**
   * @brief 构造函数
   * @param pattern 格式模板
   * @details
   *  %n-------换行符 '\n'
   *  %m-------日志内容
   *  %p-------level日志级别
   *  %r-------程序启动到现在的耗时
   *  %%-------输出一个'%'
   *  %t-------当前线程ID
   *  %T-------Tab键
   *  %tn------当前线程名称
   *  %d-------日期和时间
   *  %f-------文件名
   *  %l-------行号
   *  %g-------日志器名字
   *  %c-------当前协程ID
   *
   *  默认格式 "%d{%Y-%m-%d %H:%M:%S}%T%f:%l%T[%p]%T%c%T%t(%N)%T%F%T%m%n""
   */
  LogFormatter(const std::string &pattern);

  /**
   * @brief 返回格式化日志文本
   * @param logger 日志器
   * @param level 日志级别
   * @param event 日志事件
   */
  std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level,
                     LogEvent::ptr event);
  std::ostream &format(std::ostream &ofs, std::shared_ptr<Logger> logger,
                       LogLevel::Level level, LogEvent::ptr event);

public:
  /**
   * @brief 日志内容项格式化
   */
  class FormatItem {
  public:
    typedef std::shared_ptr<FormatItem> ptr;
    /**
     * @brief 析构函数
     */
    virtual ~FormatItem() {}
    /**
     * @brief 格式化日志到流
     * @param[in, out] os 日志输出流
     * @param logger 日志器
     * @param level 日志等级
     * @param event 日志事件
     */
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger,
                        LogLevel::Level level, LogEvent::ptr event) = 0;
  };

  /**
   * @brief 初始化,解析日志模板
   */
  void init();

  /**
   * @brief 是否有错误
   */
  bool isError() const { return m_error; }

  /**
   * @brief 返回日志模板
   */
  const std::string getPattern() const { return mLF_pattern; }

private:
  /// 日志格式模板
  std::string mLF_pattern;
  /// 日志格式解析后格式
  std::vector<FormatItem::ptr> mLF_FI_items;
  /// 是否有错误
  bool m_error = false;
};

/**
 * @brief 日志输出目标
 */
class LogAppender {
  friend class Logger;

public:
  typedef std::shared_ptr<LogAppender> ptr;
  typedef Spinlock MutexType;

  /**
   * @brief 析构函数
   */
  virtual ~LogAppender() {}

  /**
   * @brief 写入日志
   *  日志输出的多态接口。通过该多态接口，会去调用具体子类中的log()函数执行输出动作
   * @param logger 日志器
   * @param level 日志级别
   * @param event 日志事件
   */
  virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   LogEvent::ptr event) = 0;

  /**
   * @brief 将日志输出目标的配置转成YAML String
   */
  virtual std::string toYamlString() = 0;

  /**
   * @brief 更改日志格式器
   */
  void setFormatter(LogFormatter::ptr val);

  /**
   * @brief 获取日志格式器
   */
  LogFormatter::ptr getFormatter();

  /**
   * @brief 获取日志级别
   */
  LogLevel::Level getLevel() const { return m_level; }

  /**
   * @brief 设置日志级别
   */
  void setLevel(LogLevel::Level val) { m_level = val; }

protected:
  /// 日志级别
  LogLevel::Level m_level = LogLevel::DEBUG;
  /// 是否有自己的日志格式器
  bool m_hasFormatter = false;
  /// Mutex
  MutexType m_mutex;
  /// 日志格式器
  LogFormatter::ptr m_formatter;
};

/**
 * @brief 日志器
 */
class Logger : public std::enable_shared_from_this<Logger> {
  friend class LoggerManager;

public:
  typedef std::shared_ptr<Logger> ptr;
  typedef Spinlock MutexType;

  /**
   * @brief 构造函数
   * @param name 日志器名称
   */
  Logger(const std::string &name = "root");

  /**
   * @brief 写日志
   * @param level 日志级别
   * @param event 日志事件
   */
  void log(LogLevel::Level level, LogEvent::ptr event);

  /**
   * @brief 写debug级别日志
   * @param event 日志事件
   */
  void debug(LogEvent::ptr event);

  /**
   * @brief 写info级别日志
   * @param event 日志事件
   */
  void info(LogEvent::ptr event);

  /**
   * @brief 写warn级别日志
   * @param event 日志事件
   */
  void warn(LogEvent::ptr event);

  /**
   * @brief 写error级别日志
   * @param event 日志事件
   */
  void error(LogEvent::ptr event);

  /**
   * @brief 写fatal级别日志
   * @param event 日志事件
   */
  void fatal(LogEvent::ptr event);

  /**
   * @brief 添加日志目标
   * @param appender 日志目标
   */
  void addAppender(LogAppender::ptr appender);

  /**
   * @brief 删除日志目标
   * @param appender 日志目标
   */
  void delAppender(LogAppender::ptr appender);

  /**
   * @brief 清空日志目标
   */
  void clearAppenders();

  /**
   * @brief 返回日志级别
   */
  LogLevel::Level getLevel() const { return m_level; }

  /**
   * @brief 设置日志级别
   */
  void setLevel(LogLevel::Level val) { m_level = val; }

  /**
   * @brief 返回日志名称
   */
  const std::string &getName() const { return m_name; }

  /**
   * @brief 设置日志格式器
   */
  void setFormatter(LogFormatter::ptr val);

  /**
   * @brief 设置日志格式模板
   */
  void setFormatter(const std::string &val);

  /**
   * @brief 获取日志格式器
   */
  LogFormatter::ptr getFormatter();

  /**
   * @brief 将日志器的配置转成YAML String
   */
  std::string toYamlString();

private:
  /// 日志名称
  std::string m_name;
  /// 日志级别
  LogLevel::Level m_level;
  /// Mutex
  MutexType m_mutex;
  /// 日志目标集合
  std::list<LogAppender::ptr> m_appenders;
  /// 日志格式器
  LogFormatter::ptr m_formatter;
  /// 主日志器
  Logger::ptr m_root;
};

/**
 * @brief 输出到控制台的Appender
 */
class StdoutLogAppender : public LogAppender {
public:
  typedef std::shared_ptr<StdoutLogAppender> ptr;
  void log(Logger::ptr logger, LogLevel::Level level,
           LogEvent::ptr event) override;
  std::string toYamlString() override;
};

/**
 * @brief 输出到文件的Appender
 */
class FileLogAppender : public LogAppender {
public:
  typedef std::shared_ptr<FileLogAppender> ptr;
  FileLogAppender(const std::string &filename);
  void log(Logger::ptr logger, LogLevel::Level level,
           LogEvent::ptr event) override;
  std::string toYamlString() override;

  /**
   * @brief 重新打开日志文件
   * @return 成功返回true
   */
  bool reopen();

private:
  /// 文件路径
  std::string m_filename;
  /// 文件流
  std::ofstream m_filestream;
  /// 上次重新打开时间
  uint64_t m_lastTime = 0;
};

/**
 * @brief 日志器管理类
 */
class LoggerManager {
public:
  typedef Spinlock MutexType;
  /**
   * @brief 构造函数
   */
  LoggerManager();

  /**
   * @brief 获取日志器
   * @param name 日志器名称
   */
  Logger::ptr getLogger(const std::string &name);

  /**
   * @brief 初始化
   */
  void init();

  /**
   * @brief 返回主日志器
   */
  Logger::ptr getRoot() const { return m_root; }

  /**
   * @brief 将所有的日志器配置转成YAML String
   */
  std::string toYamlString();

private:
  /// Mutex
  MutexType m_mutex;
  /// 日志器容器
  std::map<std::string, Logger::ptr> m_loggers;
  /// 主日志器
  Logger::ptr m_root;
};

/// 日志器管理类单例模式
typedef bin::Singleton<LoggerManager> LoggerMgr;

} // namespace bin

#endif

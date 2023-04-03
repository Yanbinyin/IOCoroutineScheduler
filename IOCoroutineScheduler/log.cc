/**
 * @file log.cc
 * @author yinyb (990900296@qq.com)
 * @brief 日志封装
 * @version 1.0
 * @date 2022-04-02
 * @copyright Copyright (c) {2022}
 */

#include <functional>
#include <iostream>
#include <string.h>
#include <time.h>

#include "config.h"
#include "log.h"

namespace bin {

const char *LogLevel::ToString(LogLevel::Level level) {
#define XX(name)                                                               \
  case LogLevel::name:                                                         \
    return #name;                                                              \
    break;
  switch (level) {
    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX
  default:
    return "UNKNOW";
  }
  return "UNKNOW";
}

LogLevel::Level LogLevel::FromString(const std::string &str) {
#define XX(level, v)                                                           \
  if (str == #v) {                                                             \
    return LogLevel::level;                                                    \
  }
  XX(DEBUG, debug);
  XX(INFO, info);
  XX(WARN, warn);
  XX(ERROR, error);
  XX(FATAL, fatal);

  XX(DEBUG, DEBUG);
  XX(INFO, INFO);
  XX(WARN, WARN);
  XX(ERROR, ERROR);
  XX(FATAL, FATAL);
  return LogLevel::UNKNOW;
#undef XX
}

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   const char *file, int32_t line, uint32_t elapse,
                   uint32_t thread_id, uint32_t fiber_id, uint64_t time,
                   const std::string &thread_name)
    : m_file(file), m_line(line), m_elapse(elapse), m_threadId(thread_id),
      m_fiberId(fiber_id), m_time(time), m_threadName(thread_name),
      m_logger(logger), m_level(level) {}

// hack:1、怎么遍历的所有形参呢
void LogEvent::format(const char *fmt, ...) {
  va_list al;        // al char* 类型
  va_start(al, fmt); // 这个函数之后，al指向...的第一个参数
  format(fmt, al);
  va_end(al); //
}

void LogEvent::format(const char *fmt, va_list al) {
  char *buf = nullptr;
  // vasprintf会动态分配内存 失败返回-1
  int len = vasprintf(&buf, fmt, al);
  if (len != -1) {
    m_ss << std::string(buf, len);
    free(buf);
  }
}

LogEventWrap::LogEventWrap(LogEvent::ptr e) : m_event(e) {}

LogEventWrap::~LogEventWrap() {
  m_event->getLogger()->log(m_event->getLevel(), m_event);
}

std::stringstream &LogEventWrap::getSS() { return m_event->getSS(); }

void LogAppender::setFormatter(LogFormatter::ptr val) {
  MutexType::Lock lock(m_mutex);
  m_formatter = val;
  if (m_formatter) {
    m_hasFormatter = true;
  } else {
    m_hasFormatter = false;
  }
}

LogFormatter::ptr LogAppender::getFormatter() {
  MutexType::Lock lock(m_mutex);
  return m_formatter;
}

class MessageFormatItem : public LogFormatter::FormatItem {
public:
  MessageFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, std::shared_ptr<Logger> logger,
              LogLevel::Level level, LogEvent::ptr event) override {
    os << event->getContent();
  }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
  LevelFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, std::shared_ptr<Logger> logger,
              LogLevel::Level level, LogEvent::ptr event) override {
    os << LogLevel::ToString(level);
  }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
  ElapseFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, std::shared_ptr<Logger> logger,
              LogLevel::Level level, LogEvent::ptr event) override {
    os << event->getElapse();
  }
};

class NameFormatItem : public LogFormatter::FormatItem {
public:
  NameFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, std::shared_ptr<Logger> logger,
              LogLevel::Level level, LogEvent::ptr event) override {
    os << event->getLogger()->getName();
  }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
  ThreadIdFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getThreadId();
  }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
  ThreadNameFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getThreadName();
  }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
  FiberIdFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getFiberId();
  }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
  DateTimeFormatItem(const std::string &format = "%Y-%m-%d %H:%M:%S")
      : m_format(format) {
    if (m_format.empty()) {
      m_format = "%Y-%m-%d %H:%M:%S";
    }
  }

  void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    // os << event->getTime();
    struct tm tm;                   // struct tm： linux常用的时间类型
    time_t time = event->getTime(); // time_t:  linux常用的时间类型
    localtime_r(&time,
                &tm); // 将参数time所指的数据转换成从公元1970年1月1日0时0分0
                      // 秒算起至今的UTC时间所经过的秒数，并存在结果体tm中
    char buf[64];
    // c_str(): 返回当前字符串的首字符地址
    // strftime： 根据m_format格式化tm，并存储到buf中
    strftime(buf, sizeof(buf), m_format.c_str(), &tm);
    os << buf;
  }

private:
  std::string m_format;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
public:
  FileNameFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getFile();
  }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
  LineFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getLine();
  }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
  NewLineFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << std::endl;
  }
};

class StringFormatItem : public LogFormatter::FormatItem {
public:
  StringFormatItem(const std::string &str) : m_string(str) {}
  void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << m_string;
  }

private:
  std::string m_string;
};

class TabFormatItem : public LogFormatter::FormatItem {
public:
  TabFormatItem(const std::string &str = "") {}
  void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << "\t";
  }

private:
  std::string m_string;
};

Logger::Logger(const std::string &name)
    : m_name(name), m_level(LogLevel::DEBUG) {
  // m_formatter.reset(new LogFormatter("%d{%Y-%m-%d
  // %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n")); //默认格式
  m_formatter.reset(new LogFormatter(
      "%d{%Y-%m-%d %H:%M:%S}%T%f:%l%T[%p]%T%c%T%t(%N)%T%F%T%m%n")); // bin add
}

void Logger::log(LogLevel::Level level, const LogEvent::ptr event) {
  // 如果传入的日志级别 >= 当前日志器的级别都能进行输出
  if (level >= m_level) {
    // power:拿到this指针的智能指针
    auto self = shared_from_this();
    MutexType::Lock lock(m_mutex);
    if (!m_appenders.empty()) {
      for (auto &i : m_appenders)
        i->log(self, level, event);
    }
    // 如果当前的Logger没有分配LogAppender,就使用m_root中的LogAppender来打印
    else if (m_root) {
      // 递归调用自身
      m_root->log(level, event);
    }
  }
}

void Logger::debug(LogEvent::ptr event) { log(LogLevel::DEBUG, event); }

void Logger::info(LogEvent::ptr event) { log(LogLevel::INFO, event); }

void Logger::warn(LogEvent::ptr event) { log(LogLevel::WARN, event); }

void Logger::error(LogEvent::ptr event) { log(LogLevel::ERROR, event); }

void Logger::fatal(LogEvent::ptr event) { log(LogLevel::FATAL, event); }

void Logger::addAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(m_mutex);
  // 发现加入的日志输出器没有格式器的话 赋予默认格式器(m_formatter)
  if (!appender->getFormatter()) {
    MutexType::Lock ll(appender->m_mutex);
    appender->m_formatter = m_formatter;
  }
  m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(m_mutex);
  for (auto it = m_appenders.begin(); it != m_appenders.end(); it++)
    if (*it == appender) {
      m_appenders.erase(it);
      break;
    }
}

// 重载函数
// 设置日志格式模板
void Logger::setFormatter(LogFormatter::ptr val) {
  MutexType::Lock lock(m_mutex);
  m_formatter = val;

  for (auto &i : m_appenders) {
    MutexType::Lock ll(i->m_mutex);
    if (!i->m_hasFormatter) {
      i->m_formatter = m_formatter;
    }
  }
}

void Logger::setFormatter(const std::string &val) {
  bin::LogFormatter::ptr new_val(new bin::LogFormatter(val));
  // 错误的，没必要设置
  if (new_val->isError()) {
    std::cout << "Logger setFormatter name=" << m_name << " value=" << val
              << " invalid formatter" << std::endl;
    return;
  }
  // MutexType::Lock lock(m_mutex);
  // m_formatter = new_val;
  setFormatter(new_val);
}

LogFormatter::ptr Logger::getFormatter() {
  MutexType::Lock lock(m_mutex);
  return m_formatter;
}

void Logger::clearAppenders() {
  MutexType::Lock lock(m_mutex);
  m_appenders.clear();
}

std::string Logger::toYamlString() {
  // MutexType::Lock lock(m_mutex);
  YAML::Node node;
  node["name"] = m_name;
  if (m_level != LogLevel::UNKNOW) {
    node["level"] = LogLevel::ToString(m_level);
  }
  if (m_formatter) {
    node["formatter"] = m_formatter->getPattern();
  }
  for (auto &i : m_appenders) {
    node["appenders"].push_back(YAML::Load(i->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

FileLogAppender::FileLogAppender(const std::string &filename)
    : m_filestream(filename) {}

// void FileLogAppender::log(Logger::ptr logger, LogLevel::Level
// level,LogEvent::ptr event){
//     if(level>m_level)
//         m_filestream << m_formatter-> format(logger, level, event);
// }

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                          LogEvent::ptr event) {
  if (level >= m_level) {
    uint64_t now = event->getTime();
    if (now >= (m_lastTime + 3)) {
      reopen();
      m_lastTime = now;
    }
    MutexType::Lock lock(m_mutex);
    // if(!(m_filestream << m_formatter->format(logger, level, event))){
    if (!m_formatter->format(m_filestream, logger, level, event)) {
      std::cout << "error" << std::endl;
    }
  }
}

bool FileLogAppender::reopen() {
  MutexType::Lock lock(m_mutex);
  if (m_filestream)
    m_filestream.close();
  m_filestream.open(m_filename);
  return !!m_filestream; // 非0值转成1，0值还是0.
}

std::string FileLogAppender::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  node["type"] = "FileLogAppender";
  node["file"] = m_filename;
  if (m_level != LogLevel::UNKNOW) {
    node["level"] = LogLevel::ToString(m_level);
  }
  if (m_hasFormatter && m_formatter) {
    node["formatter"] = m_formatter->getPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

void StdoutLogAppender::log(Logger::ptr logger, LogLevel::Level level,
                            LogEvent::ptr event) {
  if (level >= m_level) {
    // std::cout 不是线程安全的
    MutexType::Lock lock(m_mutex);
    std::cout << m_formatter->format(logger, level, event);
  }
}

std::string StdoutLogAppender::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  node["type"] = "StdoutLogAppender";
  if (m_level != LogLevel::UNKNOW) {
    node["level"] = LogLevel::ToString(m_level);
  }
  // 如果输出器单独设置了格式 也要显示
  if (m_hasFormatter && m_formatter) {
    node["formatter"] = m_formatter->getPattern();
  }
  std::stringstream ss;
  ss << node;
  // ss << "BinTest";
  return ss.str();
}

LogFormatter::LogFormatter(const std::string &pattern) : mLF_pattern(pattern) {
  init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger,
                                 LogLevel::Level level, LogEvent::ptr event) {
  std::stringstream ss;
  for (auto &i : mLF_FI_items) {
    i->format(ss, logger, level, event);
  }
  return ss.str();
}

std::ostream &LogFormatter::format(std::ostream &ofs,
                                   std::shared_ptr<Logger> logger,
                                   LogLevel::Level level, LogEvent::ptr event) {
  for (auto &i : mLF_FI_items) {
    i->format(ofs, logger, level, event);
  }
  return ofs;
}

// 核心函数
// power:以%开头的遇到特殊字符截断，不以%开头，遇到%截断
void LogFormatter::init() {
  // str, format, type
  //  1.模板字符串 2.指定的参数字符串 3.是否为正确有效模板（0无效，1有效）
  std::vector<std::tuple<std::string, std::string, int>> vec;
  std::string nstr;
  // m_formatter.reset(new LogFormatter("%d{%Y-%m-%d
  // %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
  for (size_t i = 0; i < mLF_pattern.size(); ++i) {

    /*------------//power
        这里处理 nstr，存入符号
    -------------*/

    /*  如果 mLF_pattern[i] 是正常符号， 合并到nstr中一块压入vec
            1、本身不为%
            2、两个%连续出现，压入一个%
    */
    if (mLF_pattern[i] != '%') {
      nstr.append(1, mLF_pattern[i]);
      continue;
    }
    if ((i + 1) < mLF_pattern.size() &&
        mLF_pattern[i + 1] ==
            '%') { // power:这两个条件的顺序不能更换，先判断数组是否越界，再判断数组内容
      nstr.append(1, '%');
      continue;
    }

    /*  %单个出现，即：
            mLF_pattern[i] == '%' && mLF_pattern[i + 1] != '%'
        的条件下才会进行下面的处理
    */

    size_t n = i + 1;
    int fmt_status = 0;
    size_t fmt_begin = 0;

    std::string str;
    std::string fmt;

    /*---------------//power
        这里处理str，存入字母
        处理fmt，即格式{}中间的部分
    ---------------*/
    // power:n = i + 1; 因此这段while处理的是%后面的字符
    while (n < mLF_pattern.size()) {
      /*  break：
              1、fmt_status == 0 && mLF_pattern[n]不是字母,不为{}
              2、是 }
              字母和 { 不跳出，空格、特殊字符等都跳出
      */
      if (!fmt_status && (!isalpha(mLF_pattern[n]) && mLF_pattern[n] != '{' &&
                          mLF_pattern[n] != '}')) {
        str = mLF_pattern.substr(i + 1, n - i - 1);
        break;
      }
      if (fmt_status == 0) {
        if (mLF_pattern[n] == '{') {
          str = mLF_pattern.substr(i + 1, n - i - 1);
          // std::cout << "*" << str << std::endl;
          fmt_status = 1; // 解析格式
          fmt_begin = n;
          ++n;
          continue;
        }
      } else if (fmt_status == 1) {
        if (mLF_pattern[n] == '}') {
          fmt = mLF_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
          // std::cout << "#" << fmt << std::endl;
          fmt_status = 0;
          ++n;
          break;
        }
      }
      ++n;
      if (n == mLF_pattern.size()) {
        if (str.empty()) {
          str = mLF_pattern.substr(i + 1);
        }
      }
    }

    if (fmt_status == 0) {
      if (!nstr.empty()) {
        // vec.push_back 1:压入符号
        vec.push_back(
            std::make_tuple(nstr, std::string(""), 0)); // nstr 是符号[ ] :
        nstr.clear();
      }
      // vec.push_back 2：压入字母
      vec.push_back(std::make_tuple(str, fmt, 1)); // str 储存字母
      i = n - 1;
    } else if (fmt_status == 1) {
      // 解析出现了问题：  fmt_status == 1 表明上面while遍历时，遇到 '{'
      // 进入if赋值1，但是没有遇到 '}' 进入else if给它赋值为0 std::cout <<
      // "pattern parse error: " << mLF_pattern << " - " <<
      // mLF_pattern.substr(i) << std::endl;
      m_error = true;
      // vec.push_back 3：
      vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
    }
  }

  if (!nstr.empty()) {
    vec.push_back(std::make_tuple(nstr, "", 0));
  }
  static std::map<std::string,
                  std::function<FormatItem::ptr(const std::string &str)>>
      s_format_items = {
#define XX(str, C)                                                             \
  {                                                                            \
#str, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); }   \
  }

          XX(m, MessageFormatItem),    // m:消息
          XX(p, LevelFormatItem),      // p:日志级别
          XX(r, ElapseFormatItem),     // r:累计毫秒数
          XX(c, NameFormatItem),       // c:日志名称
          XX(t, ThreadIdFormatItem),   // t:线程id
          XX(n, NewLineFormatItem),    // n:换行
          XX(d, DateTimeFormatItem),   // d:时间
          XX(f, FileNameFormatItem),   // f:文件名
          XX(l, LineFormatItem),       // l:行号
          XX(T, TabFormatItem),        // T:Tab
          XX(F, FiberIdFormatItem),    // F:协程id
          XX(N, ThreadNameFormatItem), // N:线程名称
#undef XX
      };

  for (auto &i : vec) {
    if (std::get<2>(i) == 0) {
      mLF_FI_items.push_back(
          FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
    } else {
      auto it = s_format_items.find(std::get<0>(i));
      if (it == s_format_items.end()) {
        mLF_FI_items.push_back(FormatItem::ptr(
            new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
        m_error = true;
      } else {
        mLF_FI_items.push_back(it->second(std::get<1>(i)));
      }
    }

    // std::cout << "(" << std::get<0>(i) << ") - (" << std::get<1>(i) << ") -
    // (" << std::get<2>(i) << ")" << std::endl;
  }
  // std::cout << mLF_FI_items.size() << std::endl;
}

LoggerManager::LoggerManager() {
  // 生成一个默认的Logger 并且配置一个默认的LogAppender 输出到控制台
  m_root.reset(new Logger);
  m_root->addAppender(LogAppender::ptr(new StdoutLogAppender()));

  m_loggers[m_root->m_name] = m_root;

  init();
}
Logger::ptr LoggerManager::getLogger(const std::string &name) {
  MutexType::Lock lock(m_mutex);
  auto it = m_loggers.find(name);
  // 存在
  if (it != m_loggers.end()) {
    return it->second;
  }
  // 不存在，定义一个
  Logger::ptr logger(new Logger(name));
  logger->m_root = m_root;
  m_loggers[name] = logger;
  return logger;
}

struct LogAppenderDefine {
  int type = 0; // 1 File, 2 Stdout
  LogLevel::Level level = LogLevel::UNKNOW;
  std::string formatter;
  std::string file;

  bool operator==(const LogAppenderDefine &oth) const {
    return type == oth.type && level == oth.level &&
           formatter == oth.formatter && file == oth.file;
  }
};

struct LogDefine {
  std::string name;
  LogLevel::Level level = LogLevel::UNKNOW;
  std::string formatter;
  std::vector<LogAppenderDefine> appenders;
  // std::string file;

  bool operator==(const LogDefine &oth) const {
    return name == oth.name && level == oth.level &&
           formatter == oth.formatter && appenders == appenders;
  }

  // power:存储到set需要重载 < 号
  bool operator<(const LogDefine &oth) const { return name < oth.name; }

  bool isValid() const { return !name.empty(); }
};

template <> class LexicalCast<std::string, LogDefine> {
public:
  LogDefine operator()(const std::string &v) {
    YAML::Node n = YAML::Load(v);
    LogDefine ld;
    if (!n["name"].IsDefined()) {
      std::cout << "log config error: name is null, " << n << std::endl;
      throw std::logic_error("log config name is null");
    }
    ld.name = n["name"].as<std::string>();
    // todo:LogDefine加上 file
    // 属性，然后这里更改掉就可以更改配置的路径，但是目前有bug
    //  ld.file = n["file"].as<std::string>();
    ld.level = LogLevel::FromString(
        n["level"].IsDefined() ? n["level"].as<std::string>() : "");
    if (n["formatter"].IsDefined()) {
      ld.formatter = n["formatter"].as<std::string>();
    }
    if (n["appenders"].IsDefined()) {
      // std::cout << "==" << ld.name << " = " << n["appenders"].size() <<
      // std::endl;
      for (size_t x = 0; x < n["appenders"].size(); ++x) {
        auto a = n["appenders"][x];
        if (!a["type"].IsDefined()) {
          std::cout << "log config error: appender type is null, " << a
                    << std::endl;
          continue;
        }
        std::string type = a["type"].as<std::string>();
        LogAppenderDefine lad;
        if (type == "FileLogAppender") {
          lad.type = 1;
          if (!a["file"].IsDefined()) {
            std::cout << "log config error: fileappender file is null, " << a
                      << std::endl;
            continue;
          }
          lad.file = a["file"].as<std::string>();
          if (a["formatter"].IsDefined()) {
            lad.formatter = a["formatter"].as<std::string>();
          }
        } else if (type == "StdoutLogAppender") {
          lad.type = 2;
          if (a["formatter"].IsDefined()) {
            lad.formatter = a["formatter"].as<std::string>();
          }
        } else {
          std::cout << "log config error: appender type is invalid, " << a
                    << std::endl;
          continue;
        }
        ld.appenders.push_back(lad);
      }
    }
    return ld;
  }
};

template <> class LexicalCast<LogDefine, std::string> {
public:
  std::string operator()(const LogDefine &i) {
    YAML::Node n;
    n["name"] = i.name;
    if (i.level != LogLevel::UNKNOW) {
      n["level"] = LogLevel::ToString(i.level);
    }
    if (!i.formatter.empty()) {
      n["formatter"] = i.formatter;
    }
    for (auto &a : i.appenders) {
      YAML::Node na;
      if (a.type == 1) {
        na["type"] = "FileLogAppender";
        na["file"] = a.file;
      } else if (a.type == 2) {
        na["type"] = "StdoutLogAppender";
      }
      if (a.level != LogLevel::UNKNOW) {
        na["level"] = LogLevel::ToString(a.level);
      }
      if (!a.formatter.empty()) {
        na["formatter"] = a.formatter;
      }
      n["appenders"].push_back(na);
    }
    std::stringstream ss;
    ss << n;
    return ss.str();
  }
};

// Lookup(name, 配置项数据，描述)查询并创建了s_datas[logs]
bin::ConfigVar<std::set<LogDefine>>::ptr g_log_defines =
    bin::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

struct LogIniter {
  LogIniter() {
    g_log_defines->addListener([](const std::set<LogDefine> &old_value,
                                  const std::set<LogDefine> &new_value) {
      // g_log_defines->addListener(0xF1E231, [](const std::set<LogDefine>&
      // old_value, const std::set<LogDefine>& new_value){
      BIN_LOG_INFO(BIN_LOG_ROOT()) << "on_logger_conf_changed";
      for (auto &i : new_value) {
        auto it = old_value.find(i);
        bin::Logger::ptr logger;
        if (it == old_value.end()) { // 没找到，新增logger
          logger = BIN_LOG_NAME(i.name);
        } else { // 找到了，修改日志器   旧的日志器存在  新的日志器也存在
          if (!(i == *it)) { // 两者内容有变化
            // 找到要修改的logger
            logger = BIN_LOG_NAME(i.name);
          } else { // 新旧日志相等，无变化
            continue;
          }
        }

        // 设置或修改日志
        std::cout << "** " << i.name << " level=" << i.level << "  " << logger
                  << std::endl;
        logger->setLevel(i.level);
        if (!i.formatter.empty()) {
          logger->setFormatter(i.formatter);
        }
        // 清空原有的日志输出器 重新根据.yaml文件设置输出器
        logger->clearAppenders();
        for (auto &a : i.appenders) {
          bin::LogAppender::ptr ap;
          if (a.type == 1) { // 输出到文件
            ap.reset(new FileLogAppender(a.file));
          } else if (a.type == 2) { // 输出到控制台
            // //todo:为了通过编译，暂时修改
            // if(!bin::EnvMgr::GetInstance()->has("d")){
            // //if(!bin::LoggerMgr::GetInstance()->has("d")){
            //     ap.reset(new StdoutLogAppender);
            // }else{
            //     continue;
            // }
            ap.reset(new StdoutLogAppender);
          }
          ap->setLevel(a.level);
          if (!a.formatter.empty()) {
            LogFormatter::ptr fmt(new LogFormatter(a.formatter));
            if (!fmt->isError()) {
              ap->setFormatter(fmt);
            } else {
              std::cout << "log.name=" << i.name << " appender type=" << a.type
                        << " formatter=" << a.formatter << " is invalid"
                        << std::endl;
            }
          }
          logger->addAppender(ap);
        }
      }

      for (auto &i : old_value) {
        auto it = new_value.find(i);
        if (it == new_value.end()) {
          // 删除logger
          auto logger = BIN_LOG_NAME(i.name);
          logger->setLevel((LogLevel::Level)0);
          logger->clearAppenders();
        }
      }
      // std::cout << "struct LogIniter success" << std::endl;
    });
  }
};

// power:怎么在main函数之前或者之后执行一些东西，定义一些全局对象。构建的时候调用它的构造函数
static LogIniter __log_init;

std::string LoggerManager::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  for (auto &i : m_loggers) {
    node.push_back(YAML::Load(i.second->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

void LoggerManager::init() {}

} // namespace bin

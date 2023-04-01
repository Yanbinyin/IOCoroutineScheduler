#ifndef _SYLAR_LOG_H_
#define _SYLAR_LOG_H_

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <stdarg.h>
#include <map>
#include "util.h"
#include "singleton.h"
#include "thread.h"

//固定日志级别输出日志内容
#define SYLAR_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        bin::LogEventWrap(bin::LogEvent::ptr(new bin::LogEvent(logger, level, __FILE__, __LINE__, 0, bin::GetThreadId(),\
                bin::GetFiberId(), time(0), bin::Thread::GetName()))).getSS()

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, bin::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, bin::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, bin::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, bin::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, bin::LogLevel::FATAL)

/**
 * @brief 使用格式化方式将日志级别level的日志写入到logger
 */
//带参日志输出日志内容
#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        bin::LogEventWrap(bin::LogEvent::ptr(new bin::LogEvent(logger, level, __FILE__, __LINE__, 0, bin::GetThreadId(),\
                bin::GetFiberId(), time(0), bin::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, bin::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, bin::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, bin::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, bin::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, bin::LogLevel::FATAL, fmt, __VA_ARGS__)

//通过单例LoggerMgr--->访问到LogManager中默认的Logger实例化对象
#define SYLAR_LOG_ROOT() bin::LoggerMgr::GetInstance()->getRoot() //返回一个唯一的singleton LogManager指针

//通过日志器的名字获取日志器实体
#define SYLAR_LOG_NAME(name) bin::LoggerMgr::GetInstance()->getLogger(name)

namespace bin{

    //power:提前声明的原因，目前不懂LoggerManager提前的原因
    class Logger;
    // class LoggerManager;

    //over
    //日志级别
    class LogLevel{
    public:
        enum Level{
            UNKNOW = 0,
            DEBUG = 1,
            INFO = 2,
            WARN = 3,
            ERROR = 4,
            FATAL = 5
        };

        //将日志级别转换成文本输出 level：日志级别
        static const char* ToString(LogLevel::Level level);

        //将文本转换成日志级别  str：日志级别文本
        static LogLevel::Level FromString(const std::string& str);
    };




    //日志事件
    class LogEvent{
    public:
        typedef std::shared_ptr<LogEvent> ptr;
        
        LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file, int32_t line
            ,uint32_t elapse, uint32_t thread_id, uint32_t fiber_id, uint64_t time, const std::string& thread_name);

        const char* getFile() const { return m_file; }
        int32_t getLine() const { return m_line; }
        uint32_t getElapse() const { return m_elapse; }   //返回耗时
        uint32_t getThreadId() const { return m_threadId; }
        uint32_t getFiberId() const { return m_fiberId; }
        uint64_t getTime() const { return m_time;}
        const std::string& getThreadName() const { return m_threadName; }//power:两个const各自的作用
        std::stringstream& getSS(){ return m_ss; }
        std::string getContent() const { return m_ss.str(); }
        std::shared_ptr<Logger> getLogger() const { return m_logger; }
        LogLevel::Level getLevel() const { return m_level; }

        //功能：实现类似printf("%d", 6);这样的功能，让日志内容能够带上参数
        //power:下面这两个函数是 省略符形参 的典型应用
        //hack:... 和 va_list
        void format(const char* fmt, ...);
        void format(const char* fmt, va_list al);
    private:
        const char* m_file = nullptr;   //文件名
        int32_t m_line = 0;             //行号
        uint32_t m_elapse = 0;          //程序启动开始到现在的毫秒
        uint32_t m_threadId = 0;        //线程id
        uint32_t m_fiberId = 0;         //协程id
        uint64_t m_time = 0;            //时间戳
        std::string m_threadName;       //线程名称
        std::stringstream m_ss;         //日志内容流
        std::shared_ptr<Logger> m_logger;   //日志器
        LogLevel::Level m_level;        //日志等级
    };




    /*
        目的：//hack:为什么直接封装LogEvent不行
            封装便捷的宏函数调用时，无法使得LogEvent对象正常析构。
            封装一层包装器，在其析构函数中去实现原本想实现的功能，而又不产生内存泄漏。
            使用了一个LogEventWrap的匿名对象，由于匿名对象的生命周期只有一行，因此会马上回收空间触发析构函数。
            析构函数中就有对应日志打印语句:Logger::log()。
    */
    //日志事件包装器
    class LogEventWrap{
    public:
        LogEventWrap(LogEvent::ptr e);
        ~LogEventWrap();

        LogEvent::ptr getEvent() const {return m_event;}    //获取日志事件
        std::stringstream& getSS(); //获取日志内容流

    private:
        LogEvent::ptr m_event;  //日志事件
    };



    /*
        %n-------换行符 '\n'
        %m-------日志内容
        %p-------level日志级别
        %r-------程序启动到现在的耗时
        %%-------输出一个'%'
        %t-------当前线程ID
        %T-------Tab键
        %tn------当前线程名称
        %d-------日期和时间
        %f-------文件名
        %l-------行号
        %g-------日志器名字
        %c-------当前协程ID
    */
    //日志格式器
    class LogFormatter{
    public:
        typedef std::shared_ptr<LogFormatter> ptr;
        LogFormatter(const std::string& pattern);

        //返回格式化日志文本        //%t    %thread_id  %m%n
        std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
        std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

    public:
        //日志内容项格式化
        class FormatItem{
        public:
            typedef std::shared_ptr<FormatItem> ptr;
            ///FormatItem(const std::string& fmt=""){};
            virtual ~FormatItem(){}
            ///virtual std::string format(std::ostream& os, LogEvent::ptr event) = 0;
            //核心函数
            virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
        };

        void init();    //解析日志模板  核心函数
        const std::string getPattern() const { return mLF_pattern; }  //返回日志模板
        bool isError() const { return m_error; }    //是否有错误

    private:
        std::string mLF_pattern;                    //日志格式模板 对应模板解析对应内容
        std::vector<FormatItem::ptr> mLF_FI_items;  //解析后日志格式 具体格式输出的具体子类队列
        bool m_error = false;                       //格式模板是否有错误
    };



    //over
    //日志输出目标
    class LogAppender{
    friend class Logger;
    public:
        typedef std::shared_ptr<LogAppender> ptr;
        typedef Spinlock MutexType;
        
        //what:没有构造函数
        ~LogAppender(){}
        
        //功能：日志输出的多态接口。通过该多态接口，会去调用具体子类中的log()函数执行输出动作
        virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level,LogEvent::ptr event) = 0;
        // //power:Logger是LogAppender的友元，但是Logger::ptr还没有声明，所以不能使用
        // virtual void log(Logger::ptr logger, LogLevel::Level level,LogEvent::ptr event) = 0;
        
        //将日志输出目标的配置转成yaml 输出string
        virtual std::string toYamlString() = 0;
        
        void setFormatter(LogFormatter::ptr val);   //
        LogFormatter::ptr getFormatter();           //获取日志格式器
        LogLevel::Level getLevel() const {return m_level;}
        void setLevel(LogLevel::Level level){ m_level = level; }
        void setIsFormatter(){ m_hasFormatter = true; }
        bool getIsFormatter(){ return m_hasFormatter; }
    protected:
        //输出器日志级别
        LogLevel::Level m_level = LogLevel::DEBUG;
        //是否设置过日志格式器
        bool m_hasFormatter = false;
        //Mutex
        MutexType m_mutex;
        //日志格式器智能指针
        LogFormatter::ptr m_formatter;              
    };    



    //日志器
    //enable_shared_from_this<Logger>保证Logger在自己的成员函数里面获取自己的指针
    class Logger : public std::enable_shared_from_this<Logger> {	
    friend class LoggerManager;
    public:
        typedef std::shared_ptr<Logger> ptr;
        typedef Spinlock MutexType;

        //构造函数，在生成Logger时候会生成一个默认的LogFormatter
        Logger(const std::string& name="root");//power:核心函数
        void log(LogLevel::Level level, const LogEvent::ptr event);

        void debug(LogEvent::ptr event);
        void info(LogEvent::ptr event);
        void warn(LogEvent::ptr event);
        void error(LogEvent::ptr event);
        void fatal(LogEvent::ptr event);

        //添加日志目标  appender日志目标
        void addAppender(LogAppender::ptr appender);
        void delAppender(LogAppender::ptr appender);
        void clearAppenders();
        LogLevel::Level getLevel() const { return m_level; }
        void setLevel(LogLevel::Level level){ m_level=level; }
        const std::string& getName() const { return m_name; }        
        void setFormatter(LogFormatter::ptr val);   //设置日志格式模板
        void setFormatter(const std::string& val);
        LogFormatter::ptr getFormatter();
        std::string toYamlString();  //将日志器的配置转换成YAML String
    private:
        std::string m_name;             //日志名称，默认名称为root
        LogLevel::Level m_level;        //日志级别
        MutexType m_mutex;              //Mutex
        std::list<LogAppender::ptr> m_appenders;//日志输出器Appender指针队列
        LogFormatter::ptr m_formatter;  //日志格式器，自带的一个LogFormatter 防止LogAppender没有LogFormatter
        Logger::ptr m_root;             //主日志器，设置默认Logger
    };




    //输出到控制台的Appender
    class StdoutLogAppender:public LogAppender{
    public:
        typedef std::shared_ptr<StdoutLogAppender> ptr;
        void log(Logger::ptr logger, LogLevel::Level level,LogEvent::ptr event) override;
        std::string toYamlString() override;
    };


    //输出到文件的Appender
    class FileLogAppender: public LogAppender{
    public:
        typedef std::shared_ptr<FileLogAppender> ptr;
        //输出到文件的日志输出器类构造函数  filename：输出到的具体文件路径 
        FileLogAppender(const std::string& filename);
        void log(Logger::ptr logger, LogLevel::Level level,LogEvent::ptr event) override;
        std::string toYamlString() override;
        //重新打开文件，打开成功返回true
        //功能：在多线程情况下，有可能当前我们操作的这个文件，已经被别的线程关闭，当前线程操作时候需要重新以追加内容的方式打开文件
        bool reopen();
    private:
        std::string m_filename;     //文件路径
        std::ofstream m_filestream; //输出的文件流
        uint64_t m_lastTime = 0;    //上次打开文件时间
    };



    //日志器管理类
    //构建一个管理日志器的"池"，能够管理输出不同地方的日志器，或者拥有不同权限的日志器
    class LoggerManager{
    public:
        typedef Spinlock MutexType;
        LoggerManager();

        //通过传入日志器的名称，获取具体的日志器对象智能指针
        Logger::ptr getLogger(const std::string& name);

        void init();    //初始化 空函数体
        Logger::ptr getRoot() const { return m_root; }
        std::string toYamlString();
    private:
        MutexType m_mutex;
        std::map<std::string, Logger::ptr> m_loggers;   //日志器容器  建立名字和日志器的映射关系
        Logger::ptr m_root;     //默认日志器
    };


    typedef bin::Singleton<LoggerManager> LoggerMgr;
}
#endif

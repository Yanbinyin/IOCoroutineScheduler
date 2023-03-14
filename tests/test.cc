#include<iostream>
//#include<thread>
#include"../server-bin/log.h"
#include"../server-bin/util.h"

using namespace sylar;

int main(){
    //实例化一个日志器 
    Logger::ptr logger(new Logger);

    //控制台输出器加入队列
    logger->addAppender(LogAppender::ptr(new StdoutLogAppender));
    //文件输出器加入队列 默认级别:DEBUG
    logger->addAppender(LogAppender::ptr(new FileLogAppender("../bin/doc/test_log.txt")));

    //文件输出器加入队列 设置级别:ERROR、指定模板
    LogAppender::ptr fmt_file_app(new FileLogAppender("../bin/doc/fmt_test_log.txt"));
    LogFormatter::ptr fmt(new LogFormatter("[%p]%T%d%T%m%T%n"));
    fmt_file_app->setFormatter(fmt);
    fmt_file_app->setLevel(LogLevel::ERROR);
    logger->addAppender(fmt_file_app);

    SYLAR_LOG_DEBUG(logger) << "hello log";
    SYLAR_LOG_FATAL(logger) << "有致命错误";

    SYLAR_LOG_FMT_ERROR(logger, "fmt error test: %s", "成功");
    SYLAR_LOG_WARN(logger) << "warn log";
}

// int main(){

//     sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("./doc/log.txt"));
//     sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%p%T%m%n"));
//     file_appender->setFormatter(fmt);
//     file_appender->setLevel(sylar::LogLevel::UNKNOW);



//     sylar::Logger::ptr logger(new sylar::Logger);
//     //logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));

//     logger->addAppender(file_appender);

//     // sylar::LogEvent::ptr event( new sylar::LogEvent( __FILE__, __LINE__, sylar::getThreadId(), sylar::GetFiberId(),0, 1, 2, time(0))," " );    
//     // sylar::LogEvent::ptr event( new sylar::LogEvent( __FILE__, __LINE__, 0, std::this_thread::get_id(), 2, time(0))," " );
//     // sylar::LogEvent::ptr event( new sylar::LogEvent( __FILE__, __LINE__, 0, sylar::GetThreadId(), sylar::GetFiberId(), time(0)));
//     //logger->log(sylar::LogLevel::DEBUG, event);//输出


//     std::cout << "Hello, Bin log" << std::endl ;

//     SYLAR_LOG_INFO(logger) << "test macro1";
//     SYLAR_LOG_INFO(logger) << "test macro2";
//     SYLAR_LOG_INFO(logger) << "test macro3";
//     SYLAR_LOG_ERROR(logger) << "test macro error";
//     SYLAR_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");

//     auto l = sylar::LoggerMgr::GetInstance()->getLogger("xx");
//     SYLAR_LOG_INFO(l) << "xxx";

//     // SYLAR_LOG_INFO(file_appender) << "test macro";
//     // SYLAR_LOG_ERROR(file_appender) << "test macro error";
//     // SYLAR_LOG_FMT_ERROR(file_appender, "test macro fmt error %s", "aa");

//     return 0;
// }
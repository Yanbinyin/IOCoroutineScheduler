#include<iostream>
//#include<thread>
#include"IOCoroutineScheduler/log.h"
#include"IOCoroutineScheduler/util.h"

using namespace bin;

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

    BIN_LOG_DEBUG(logger) << "hello log";
    BIN_LOG_FATAL(logger) << "有致命错误";

    BIN_LOG_FMT_ERROR(logger, "fmt error test: %s", "成功");
    BIN_LOG_WARN(logger) << "warn log";
}

// int main(){

//     bin::FileLogAppender::ptr file_appender(new bin::FileLogAppender("./doc/log.txt"));
//     bin::LogFormatter::ptr fmt(new bin::LogFormatter("%d%T%p%T%m%n"));
//     file_appender->setFormatter(fmt);
//     file_appender->setLevel(bin::LogLevel::UNKNOW);



//     bin::Logger::ptr logger(new bin::Logger);
//     //logger->addAppender(bin::LogAppender::ptr(new bin::StdoutLogAppender));

//     logger->addAppender(file_appender);

//     // bin::LogEvent::ptr event( new bin::LogEvent( __FILE__, __LINE__, bin::getThreadId(), bin::GetFiberId(),0, 1, 2, time(0))," " );    
//     // bin::LogEvent::ptr event( new bin::LogEvent( __FILE__, __LINE__, 0, std::this_thread::get_id(), 2, time(0))," " );
//     // bin::LogEvent::ptr event( new bin::LogEvent( __FILE__, __LINE__, 0, bin::GetThreadId(), bin::GetFiberId(), time(0)));
//     //logger->log(bin::LogLevel::DEBUG, event);//输出


//     std::cout << "Hello, Bin log" << std::endl ;

//     BIN_LOG_INFO(logger) << "test macro1";
//     BIN_LOG_INFO(logger) << "test macro2";
//     BIN_LOG_INFO(logger) << "test macro3";
//     BIN_LOG_ERROR(logger) << "test macro error";
//     BIN_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");

//     auto l = bin::LoggerMgr::GetInstance()->getLogger("xx");
//     BIN_LOG_INFO(l) << "xxx";

//     // BIN_LOG_INFO(file_appender) << "test macro";
//     // BIN_LOG_ERROR(file_appender) << "test macro error";
//     // BIN_LOG_FMT_ERROR(file_appender, "test macro fmt error %s", "aa");

//     return 0;
// }
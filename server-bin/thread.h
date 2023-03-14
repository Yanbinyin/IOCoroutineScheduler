//线程相关的封装
/*
    std::thread也是基于pthread实现的
    c++11的std::thread没有提供读写锁，服务器高并发条件下很多情况读多写少，没有读写锁性能损失很大
*/
#ifndef __SYLAR_THREAD_H__
#define __SYLAR_THREAD_H__


#include "mutex.h"

////自己加的为了调试 
// #include <thread>
// #include <functional>
// #include <memory>
// #include <pthread.h>
// #include "noncopyable.h"

namespace sylar
{

    //线程类
    //power:互斥量 信号量 禁止拷贝
    class Thread : Noncopyable{
    public:
        typedef std::shared_ptr<Thread> ptr;

        //构造函数 cb 线程执行函数 name 线程名称
        Thread(std::function<void()> cb, const std::string &name);
        ~Thread();

        pid_t getId() const { return m_id; }                    //线程ID
        pthread_t getThread() const { return m_thread; }        //bin add return m_thread
        const std::string &getName() const { return m_name; }   //线程名称
        

        //等待线程执行完成
        void join();

        static Thread* GetThis();                     //获取当前的线程指针
        static const std::string& GetName();          //获取当前的线程名称
        static void SetName(const std::string& name); //设置当前线程名称

    private:
        static void *run(void *arg);    //线程执行函数

    private:
        pid_t m_id = -1;            //线程id(int)  run()中初始化，用户态的线程ID和内核线程ID不是一个概念 调试时候需要拿到内核中的ID
        pthread_t m_thread = 0;     //线程结构(unsigned long), pthread_create()中初始化
        std::function<void()> m_cb; //线程执行函数
        std::string m_name;         //线程名称
        Semaphore m_semaphore;      //信号量
    };

}

#endif
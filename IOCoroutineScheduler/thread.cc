#include "thread.h"
#include "log.h"
#include "util.h"

namespace bin {

    //线程局部变量，thread_local是C++11引入的类型符
    static thread_local Thread* t_thread = nullptr; //存储当前运行线程指针
    static thread_local std::string t_thread_name = "UNKNOW";   //存储当前运行线程的名称

    //系统日志统一打印到system
    static bin::Logger::ptr g_logger = BIN_LOG_NAME("system");

    Thread* Thread::GetThis(){
        return t_thread;
    }

    const std::string& Thread::GetName(){
        return t_thread_name;
    }

    void Thread::SetName(const std::string& name){
        if(name.empty()){
            return;
        }
        if(t_thread){
            t_thread->m_name = name;
        }
        t_thread_name = name;
    }

    /*power:
    原理：在调用pthread_create()线程API的时候，传入参数 this指针 ，就能通过这个指针去访问到类中的成员，
        否则类成员静态方法中无法使用非静态成员变量。或者，另外写一个普通成员函数，在线程回调函数中去调用那个函数也能解决该问题
    */
    //功能：利用pthread库开启运行线程，并且置一个信号量去等待线程完全开启后再退出构造函数
    Thread::Thread(std::function<void()> cb, const std::string& name)
            :m_cb(cb), m_name(name){
        if(name.empty())
            m_name = "UNKNOW";
        
        //创建线程，这里初始化了m_thread
        //power:利用pthread库开启运行线程，并且置一个信号量去等待 线程完全开启后 再 退出构造函数
        int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
        if(rt){
            BIN_LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt << " name=" << name;
            throw std::logic_error("pthread_create error");
        }
        //当执行完上面的API线程可能不会马上运行 手动等待一下直到线程完全开始运行
        m_semaphore.wait();
    }


    Thread::~Thread(){
        if(m_thread){
            //析构时候不马上杀死线程 而是将其置为分离态，然后析构
            pthread_detach(m_thread);
            //BIN_LOG_INFO(g_logger) << "线程detach & 析构";
        }else{
            //BIN_LOG_INFO(g_logger) << "线程析构";
        }
    }

    void Thread::join(){
        if(m_thread){
            int rt = pthread_join(m_thread, nullptr); //以阻塞态的形式等待线程终止
            if(rt){
                BIN_LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt << " name=" << m_name;
                throw std::logic_error("pthread_join error");
            }
            m_thread = 0;
        }
        //BIN_LOG_INFO(g_logger) << "name: " << m_name << " join";   //for test, bin add
    }

    /*//power:必须为static
        void* 的，无类型指针，保证线程能够接受任意类型的参数，到时候再强制转换
    */
    void* Thread::run(void* arg){
        BIN_LOG_INFO(g_logger) << "thread::run() begin";

        //pthread_create()的时候最后一个参数为run()的参数，构造函数调用时传入的是this指针
        //接收传入的this指针 因为是个static方法 因此依靠传入this指针很重要
        Thread* thread = (Thread*)arg;
        
        //初始化线程局部变量
        t_thread = thread;
        t_thread_name = thread->m_name;
        
        //获取内核线程ID
        thread->m_id = bin::GetThreadId();
        
        //设置线程的名称,只能接收小于等于16个字符
        pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

        //防止m_cb中智能指针的引用不被释放 交换一次会减少引用次数
        std::function<void()> cb;
        cb.swap(thread->m_cb);  //防止函数有智能指针时，它的引用一直不被释放掉

        thread->m_semaphore.notify(); //确保线程已经完全初始化好了 再进行唤醒

        cb();   //执行函数
        return 0;
    }

}

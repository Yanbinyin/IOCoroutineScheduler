//基于Epoll的IO协程调度器
/*
    封装套接字句柄对象+事件对象
    目的：方便归纳句柄、事件所具有的属性。句柄带有事件，事件依附于句柄。只有句柄上才有事件触发

    注意：和HOOK模块的文件句柄类做一个区分，这里的套接字句柄专门针对套接字上的事件做回调管理
*/
#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace bin {

    //基于Epoll的IO协程调度器
    class IOManager : public Scheduler, public TimerManager {
    public:
        typedef std::shared_ptr<IOManager> ptr;
        typedef RWMutex RWMutexType;

        //IO事件
        enum Event{
            NONE = 0x0,     // 无事件
            READ = 0x1,     //读事件(EPOLLIN)
            WRITE = 0x4,    // 写事件(EPOLLOUT)
        };

    private:
        //句柄对象结构体
        //Socket事件上线文类
        struct FdContext {
            typedef Mutex MutexType;
            
            //事件对象结构体
            //事件上线文类
            struct EventContext {
                Scheduler* scheduler = nullptr; //执行事件的调度器。支持多线程、多协程、多个协程调度器，指定which
                Fiber::ptr fiber;               //事件绑定的协程
                std::function<void()> cb;       //事件的绑定回调函数
            };
            
            EventContext& getContext(Event event);  //根据event类型获取句柄对象上对应的事件对象
            void resetContext(EventContext& ctx);   //清空句柄对象(ctx)上的事件对象
            void triggerEvent(Event event);         //主动触发事件(event)，执行事件对象上的回调函数

            int fd = 0;         //事件关联的句柄//句柄/文件描述符
            Event events = NONE;//当前的事件//句柄上注册好的事件
            EventContext read;  //读事件上下文//句柄上的读事件对象
            EventContext write; //写事件上下文//句柄上的写事件对象
            MutexType mutex;    //事件的Mutex
        };

    public:
        //构造析构函数  threads: 线程数量   use_caller: 是否将调用线程包含进去  name: 调度器的名称
        IOManager(size_t threads_size = 1, bool use_caller = true, const std::string& name = "");
        ~IOManager();
	
	    //fd: socket句柄  event: 事件类型  cb: 事件回调函数
        //添加事件 0成功,-1失败 fd 给哪一个句柄fd添加 event 读/写事件
        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
	    bool delEvent(int fd, Event event);	    //删除事件，不会触发事件
        bool cancelEvent(int fd, Event event);	//取消事件，如果事件存在则触发事件
        bool cancelAll(int fd);			        //取消所有事件
        
        static IOManager* GetThis();    //返回当前的IOManager
        
    protected:
        //override
	    void tickle() override;
	    bool stopping() override;
        void idle() override;
        void onTimerInsertedAtFront() override;

        //重置socket句柄上下文的容器(m_fdContexts)大小
        void contextResize(size_t size);
        //判断是否可以停止 timeout: 最近要出发的定时器事件间隔
        bool stopping(uint64_t& timeout);

    private:
        int m_epfd = 0;     //epoll 文件句柄
        int m_tickleFds[2]; //pipe 文件句柄
        std::atomic<size_t> m_pendingEventCount = {0};  //当前等待执行的事件数量
        std::vector<FdContext*> m_fdContexts;   //socket事件上下文的容器
        RWMutexType m_mutex;
    };

}

#endif
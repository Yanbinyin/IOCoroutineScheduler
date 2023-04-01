#include "coroutine.h"
#include "config.h"
#include "macro.h"
#include "log.h"
#include "scheduler.h"
#include <atomic>

namespace bin {

    static bin::Logger::ptr g_logger = BIN_LOG_NAME("system");
    
    //记录协程信息的线程局部变量

    //协程ID累加器
    static std::atomic<uint64_t> s_fiber_id {0};
    //当前(所有)线程下存在协程的总数
    static std::atomic<uint64_t> s_fiber_count {0};

    //当前线程下正在运行协程
    static thread_local Fiber* t_fiber = nullptr;
    //上一次切出的协程
    static thread_local Fiber::ptr t_threadFiber = nullptr;
    
    //配置项 每个协程的栈默认大小为1MB
    static ConfigVar<uint32_t>::ptr g_fiber_stack_size = Config::Lookup<uint32_t>("fiber.stack_size", 128 * 1024, "fiber stack size");




    // 协程栈内存分配器类
    class MallocStackAllocator {
    public:
        //分配内存  size所需内存大小
        static void* Alloc(size_t size){
            return malloc(size);
        }        
        //释放内存  vp栈空间指针  size栈空间大小
        static void Dealloc(void* vp, size_t size){ //size为了支持mmap
            return free(vp);
        }
    };

    using StackAllocator = MallocStackAllocator;




    Fiber::Fiber(){
        BIN_LOG_DEBUG(g_logger) << "协程构造: main";
        m_state = EXEC;
        SetThis(this);
        ++s_fiber_count;

        if(getcontext(&m_ctx)){
            BIN_ASSERT2(false, "getcontext");
        }
    }

    Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller)
        :m_id(++s_fiber_id), m_cb(cb){
        BIN_LOG_DEBUG(g_logger) << "协程构造: " << m_id;
        ++s_fiber_count;

        //为协程分配栈空间 让回调函数在对应栈空间去运行  未设置的时候 stacksize = 128 * 1024
        m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();
        m_stack = StackAllocator::Alloc(m_stacksize);
        if(getcontext(&m_ctx)){
            BIN_ASSERT2(false, "getcontext");
        }
        
        //初始化上下文结构体的成员
        m_ctx.uc_link = nullptr;    //后续程序上下文
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;
        
        //use_call 标识当前的协程是否是调度协程
        if(!use_caller) //false
            makecontext(&m_ctx, &Fiber::MainFunc, 0);
        else //true
            makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);
    }

    Fiber::~Fiber(){
        --s_fiber_count;
        if(m_stack){   //有栈，回收栈
            //只要不是运行态 或者 挂起就释放栈空间
            BIN_ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT);
            //释放栈空间
            StackAllocator::Dealloc(m_stack, m_stacksize);
            BIN_LOG_DEBUG(g_logger) << "协程析构: id/s_fiber_count = " << m_id << "/" << s_fiber_count;
        }
        else{ //没有栈是主协程
            BIN_ASSERT(!m_cb);
            BIN_ASSERT(m_state == EXEC);

            Fiber* cur = t_fiber;
            if(cur == this){
                SetThis(nullptr);
            }
            BIN_LOG_DEBUG(g_logger) << "协程析构: main, s_fiber_count = " << s_fiber_count;  //m_id为0，因此不必输出id
        }
    }



    //重置协程函数，并重置状态
    //INIT，TERM, EXCEPT
    void Fiber::reset(std::function<void()> cb){
        BIN_ASSERT(m_stack);
        BIN_ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT);
        m_cb = cb;
        if(getcontext(&m_ctx)){
            BIN_ASSERT2(false, "getcontext"); 
        }

        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;

        makecontext(&m_ctx, &Fiber::MainFunc, 0);
        m_state = INIT;
    }

    // //bin:看语雀，为了改bug，根据不同的切入选择不同的切出
    // void Fiber::swapOut(){
    //     //如果当前不在调度协程上执行代码 说明是从调度协程切过来的 要切回调度协程
    //     if(this != Scheduler::GetMainFiber()){
    //         SetThis(Scheduler::GetMainFiber());
    //         if(swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx) < 0){
    //         //if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)){
    //             BIN_LOG_ERROR(g_logger) << "swapOut: swapcontext error1";
    //             BIN_ASSERT2(false, "swapcontext error1");
    //         }
    //     }
    //     //如果当前在调度协程上执行代码 说明是从init协程切过来的 要切回init协程
    //     else{
    //         SetThis(t_threadFiber.get());
    //         if(swapcontext(&m_ctx, &t_threadFiber->m_ctx) < 0){
    //             BIN_LOG_ERROR(g_logger) << "swapOut: swapcontext error2";
    //             BIN_ASSERT2(false, "swapcontext error2");
    //         }
    //     }
    // }

    ////将当前协程切换到运行状态
    //核心函数：利用swapcontext(参数1, 参数2)，将目标代码段（参数2）推送到CPU上执行，将当前执行的代码段保存起来（保存到参数1
    //母协程init-------------------->子协程
    void Fiber::swapIn(){
        //将当前的子协程Fiber* 设置到 t_fiber 中，表明是这个协程正在运行
        SetThis(this);

        //没在运行态才能调入运行
        BIN_ASSERT(m_state != EXEC);
        m_state = EXEC;
        if(swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)){
        // if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)){ //test_coroutine时取消注释这里
            BIN_LOG_ERROR(g_logger) << "swapIn: swapcontext error";
            BIN_ASSERT2(false, "swapcontext");
        }
        //BIN_LOG_INFO(g_logger) << "Fiber " << m_id << " swapin end";
    }

    //切换到后台执行
    //子协程-------------------->母协程init    
    void Fiber::swapOut(){
        SetThis(Scheduler::GetMainFiber());
        if(swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)){
        // if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)){  //test_coroutine时取消注释这里
            BIN_LOG_ERROR(g_logger) << "swapOut: swapcontext error";
            BIN_ASSERT2(false, "swapcontext");
        }
    }

    //从init协程 切换到 目标代码
    void Fiber::call(){
        SetThis(this);
        m_state = EXEC;
        if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)){
            BIN_ASSERT2(false, "swapcontext");
        }
    }

    //目标代码 切换到 从init协程
    void Fiber::back(){
        SetThis(t_threadFiber.get());
        if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)){
            BIN_ASSERT2(false, "swapcontext");
        }
    }   
    
    
    //设置当前协程
    void Fiber::SetThis(Fiber* f){
        t_fiber = f;
    }

    //返回当前协程，如果当前线程没有协程，会初始化一个主协程
    Fiber::ptr Fiber::GetThis(){
        if(t_fiber){
            return t_fiber->shared_from_this();
        }
        
        //如果当前线程没有协程，会初始化一个主协程
        //可以封装成一个函数Init()
        //初始化母协程init，为线程创建第一个协程
        Fiber::ptr main_fiber(new Fiber); //创建母协程init
        BIN_ASSERT(t_fiber == main_fiber.get());
        t_threadFiber = main_fiber; //这句代码很关键

        return t_fiber->shared_from_this();
    }

    //协程切换到后台，并且设置为Ready状态
    void Fiber::YieldToReady(){
        Fiber::ptr cur = GetThis();
        BIN_ASSERT(cur->m_state == EXEC);
        cur->m_state = READY;
        cur->swapOut();
    }

    //协程切换到后台，并且设置为Hold状态
    void Fiber::YieldToHold(){
        Fiber::ptr cur = GetThis();
        BIN_ASSERT(cur->m_state == EXEC);
        //不debug协程类时注释下面这一句代码，交给Secheduler::idle()处理，不是停止就hold住
        //cur->m_state = HOLD; 
        cur->swapOut();
    }

    //总协程数
    uint64_t Fiber::TotalFibers(){
        return s_fiber_count;
    }

    uint64_t Fiber::GetFiberId(){
        if(t_fiber){
            return t_fiber->getId();
        }
        return 0;
    }

    //usercall = false
    void Fiber::MainFunc(){
        Fiber::ptr cur = GetThis();
        BIN_ASSERT(cur);
        try{
            BIN_LOG_DEBUG(g_logger) << "Fier::MainFunc() : cb() begin";
            cur->m_cb();
            BIN_LOG_DEBUG(g_logger) << "MainFunc() : cb() end";
            cur->m_cb = nullptr;
            cur->m_state = TERM;//协程已经执行完毕
        }catch(std::exception& ex){
            cur->m_state = EXCEPT;
            BIN_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what() << " fiber_id=" << cur->getId() << std::endl
                << bin::BacktraceToString();
        }catch(...){
            cur->m_state = EXCEPT;
            BIN_LOG_ERROR(g_logger) << "Fiber Except, but dont know reason: fiber_id=" << cur->getId() << std::endl
                << bin::BacktraceToString();
        }

        //power:这里和线程确实不同 线程会自己回来  协程要手动帮他设置好回来
        //因为线程是操作系统帮忙调度的啊，协程要自己控制
        auto raw_ptr = cur.get(); 
        cur.reset(); //因为swapout切出了，不会切回来了，无法执行析构函数。让其减少一次该函数调用中应该减少的引用次数

        raw_ptr->swapOut();

        //不会再回到这个地方 回来了说明有问题
        BIN_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
    }

    //usercall = true
    void Fiber::CallerMainFunc(){
        Fiber::ptr cur = GetThis();
        BIN_ASSERT(cur);
        try{
            BIN_LOG_DEBUG(g_logger) << "Fiber::CallerMainFunc() : cb() begin";
            cur->m_cb();
            BIN_LOG_DEBUG(g_logger) << "CallerMainFunc() : cb() end";
            cur->m_cb = nullptr;
            cur->m_state = TERM; //协程已经执行完毕
        }catch(std::exception& ex){
            cur->m_state = EXCEPT;
            BIN_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                << " fiber_id=" << cur->getId() << std::endl
                << bin::BacktraceToString();
        }catch(...){
            cur->m_state = EXCEPT;
            BIN_LOG_ERROR(g_logger) << "Fiber Except, but dont know reason: "
                << " fiber_id=" << cur->getId() << std::endl
                << bin::BacktraceToString();
        }
        
        auto raw_ptr = cur.get(); //引入一个普通指针
        cur.reset(); //让其减少一次该函数调用中应该减少的引用次数

        raw_ptr->back();

        //不会再回到这个地方 回来了说明有问题
        BIN_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
    }

}
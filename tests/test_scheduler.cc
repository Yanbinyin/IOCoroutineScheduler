#include "../server-bin/bin.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
// static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

void func1(){
    SYLAR_LOG_INFO(g_logger) << "func1() work!!!!!!!!!11111111";
}

void func2(){
    SYLAR_LOG_INFO(g_logger) << "func2() work!!!!!!!!!22222222";
}

void func3(){
    SYLAR_LOG_INFO(g_logger) << "func3() work!!!!!!!!!33333333";
}


//block0: 
#if 0
//同一个线程里面 可以 创建多个 不纳入调度器的scheduler
void createScheduler1(const std::string& name){
    sylar::Scheduler s1(2, false, name);
    sylar::Scheduler s2(2, false, name);
}

//同一个线程里面 不能 创建多个 纳入调度器的scheduler
void createScheduler2(const std::string& name){
    sylar::Scheduler s1(2, true, name + "1");
    sylar::Scheduler s2(2, true, name + "2");
}

int main(){
    std::vector<sylar::Thread::ptr> thrs;   //创建线程池
    for(size_t i = 0; i < 3; ++i){
        std::function<void()> func = std::bind(&createScheduler1, "s" + std::to_string(i));
        //std::function<void()> func = std::bind(&createScheduler2, "s" + std::to_string(i));
        sylar::Thread::ptr thr(new sylar::Thread(func, "t" + std::to_string(i)));
        thrs.push_back(thr);
    }
    for(auto& i : thrs)
        i->join();
    return 0;
}
#endif


//block1: 查看基本逻辑 并 对比调度器 true 和 false
#if 0
int main(){
	SYLAR_LOG_INFO(g_logger) << "main begin";

    //sb
	sylar::Scheduler bc(1, false, "testFalse"); //false
    //SYLAR_LOG_INFO(g_logger) << "bc.start() begin";
	bc.start();
    //SYLAR_LOG_INFO(g_logger) << "bc.stop() begin";
	bc.stop();

    printLine();
    
    //sc
	sylar::Scheduler sc(1, true, "testTrue");  //true
	//SYLAR_LOG_INFO(g_logger) << "sc.start() begin";
	sc.start(); //开启调度器
	//SYLAR_LOG_INFO(g_logger) << "sc.stop() begin";
	sc.stop();  //结束调度器

	SYLAR_LOG_INFO(g_logger) << "over";
    
	return 0;
}
#endif


//block2:sylar 的原始test文件
#if 0
void test_fiber(){
    static int s_count = 3;
    SYLAR_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    sleep(1);
    if(--s_count >= 0){
        sylar::Scheduler::GetThis()->schedule(&test_fiber, sylar::GetThreadId());
    }
}

int main(int argc, char** argv){
    SYLAR_LOG_INFO(g_logger) << "main";
    sylar::Scheduler sc(3, false, "test");
    sc.start();
    sleep(2);
    printLine();
    SYLAR_LOG_INFO(g_logger) << "schedule";
    //sc.schedule(&test_fiber);
    sc.stop();
    SYLAR_LOG_INFO(g_logger) << "over";
    return 0;
}
#endif




//block3: 简单调度函数还有协程
#if 0
void invokeFunc(){
    sylar::Scheduler sf(1, false, "test1");   //将调度器命名为"test"

    sf.schedule(&func1);	//以函数形式入队	

    sf.start(); //开启调度器
    sleep(1);
    sf.stop(); //结束调度器

    printLine("invokeFunc end:");
}

void invokeFiber(){
    sylar::Scheduler sc(1, false, "test2");   //将调度器命名为"test"

    sylar::Fiber::ptr c1(new sylar::Fiber(&func1));	//创建协程
    sc.schedule(c1);		//以协程的形式入队	

    sc.start(); //开启调度器
    sleep(1);
    sc.stop(); //结束调度器

    printLine("invokeFiber end:");
}

void invokeBoth(){
    sylar::Scheduler sc(1, false, "test3");   //将调度器命名为"test"

    sc.schedule(&func1);	//以函数形式入队
    sylar::Fiber::ptr c1(new sylar::Fiber(&func2));	//创建协程
    sc.schedule(c1);		//以协程的形式入队	

    sc.start(); //开启调度器

    sc.schedule(&func3); //看清基本逻辑后取消注释这里，查看调度器开启之后添加任务fun3()

    sleep(1);
    sc.stop(); //结束调度器

    printLine("main end:");
}

int main(){
    //invokeFunc(); sleep(2); printLine();
    //invokeFiber(); sleep(2); printLine();
    invokeBoth(); sleep(2); printLine();
    return 0;
}
#endif



//block4:
#if 0
void test_fiber(){
    SYLAR_LOG_INFO(g_logger) << "test in fiber s_count";
    ::sleep(1); //error: 休眠就段错误，读写锁报错

    static int s_count = 5;
    if(--s_count >= 0){
        //SYLAR_LOG_INFO(g_logger) << "调度" << s_count;
        sylar::Scheduler::GetThis()->schedule(&test_fiber, sylar::GetThreadId());
    }
}

int main(int argc, char** argv){
    SYLAR_LOG_INFO(g_logger) << "main";
    // sylar::Scheduler sc(3, false, "test");
    sylar::Scheduler sc(1, false, "test");
    sc.start();

    SYLAR_LOG_INFO(g_logger) << "sleep2" << std::endl;

    SYLAR_LOG_INFO(g_logger) << "schedule" << std::endl;
    sc.schedule(&test_fiber);

    SYLAR_LOG_INFO(g_logger) << "stop" << std::endl;
    sc.stop();

    SYLAR_LOG_INFO(g_logger) << "over";
    return 0;
}
#endif


void test_fiber() {
    //static int s_count = 3;
    static int s_count = 2;
    SYLAR_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    sleep(1);
    if(--s_count >= 0) {
        sylar::Scheduler::GetThis()->schedule(&test_fiber, sylar::GetThreadId());
    }
}

int main(int argc, char** argv) {
    SYLAR_LOG_INFO(g_logger) << "main";
    // sylar::Scheduler sc(3, false, "test");
    sylar::Scheduler sc(1, false, "test");
    sc.start();

    SYLAR_LOG_INFO(g_logger) << "sleep2" << std::endl;
    sleep(2);

    SYLAR_LOG_INFO(g_logger) << "schedule" << std::endl;
    sc.schedule(&test_fiber);
    // sleep(2);

    SYLAR_LOG_INFO(g_logger) << "stop" << std::endl;
    sc.stop();

    SYLAR_LOG_INFO(g_logger) << "over";
    return 0;
}

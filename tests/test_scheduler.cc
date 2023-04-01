#include "../IOCoroutineScheduler/bin.h"

bin::Logger::ptr g_logger = BIN_LOG_ROOT();
// static bin::Logger::ptr g_logger = BIN_LOG_NAME("system");

void func1(){
    BIN_LOG_INFO(g_logger) << "func1() work!!!!!!!!!11111111";
}

void func2(){
    BIN_LOG_INFO(g_logger) << "func2() work!!!!!!!!!22222222";
}

void func3(){
    BIN_LOG_INFO(g_logger) << "func3() work!!!!!!!!!33333333";
}


//block0: 
#if 0
//同一个线程里面 可以 创建多个 不纳入调度器的scheduler
void createScheduler1(const std::string& name){
    bin::Scheduler s1(2, false, name);
    bin::Scheduler s2(2, false, name);
}

//同一个线程里面 不能 创建多个 纳入调度器的scheduler
void createScheduler2(const std::string& name){
    bin::Scheduler s1(2, true, name + "1");
    bin::Scheduler s2(2, true, name + "2");
}

int main(){
    std::vector<bin::Thread::ptr> thrs;   //创建线程池
    for(size_t i = 0; i < 3; ++i){
        std::function<void()> func = std::bind(&createScheduler1, "s" + std::to_string(i));
        //std::function<void()> func = std::bind(&createScheduler2, "s" + std::to_string(i));
        bin::Thread::ptr thr(new bin::Thread(func, "t" + std::to_string(i)));
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
	BIN_LOG_INFO(g_logger) << "main begin";

    //sb
	bin::Scheduler bc(1, false, "testFalse"); //false
    //BIN_LOG_INFO(g_logger) << "bc.start() begin";
	bc.start();
    //BIN_LOG_INFO(g_logger) << "bc.stop() begin";
	bc.stop();

    printLine();
    
    //sc
	bin::Scheduler sc(1, true, "testTrue");  //true
	//BIN_LOG_INFO(g_logger) << "sc.start() begin";
	sc.start(); //开启调度器
	//BIN_LOG_INFO(g_logger) << "sc.stop() begin";
	sc.stop();  //结束调度器

	BIN_LOG_INFO(g_logger) << "over";
    
	return 0;
}
#endif


//block2:bin 的原始test文件
#if 0
void test_fiber(){
    static int s_count = 3;
    BIN_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    sleep(1);
    if(--s_count >= 0){
        bin::Scheduler::GetThis()->schedule(&test_fiber, bin::GetThreadId());
    }
}

int main(int argc, char** argv){
    BIN_LOG_INFO(g_logger) << "main";
    bin::Scheduler sc(3, false, "test");
    sc.start();
    sleep(2);
    printLine();
    BIN_LOG_INFO(g_logger) << "schedule";
    //sc.schedule(&test_fiber);
    sc.stop();
    BIN_LOG_INFO(g_logger) << "over";
    return 0;
}
#endif




//block3: 简单调度函数还有协程
#if 0
void invokeFunc(){
    bin::Scheduler sf(1, false, "test1");   //将调度器命名为"test"

    sf.schedule(&func1);	//以函数形式入队	

    sf.start(); //开启调度器
    sleep(1);
    sf.stop(); //结束调度器

    printLine("invokeFunc end:");
}

void invokeFiber(){
    bin::Scheduler sc(1, false, "test2");   //将调度器命名为"test"

    bin::Fiber::ptr c1(new bin::Fiber(&func1));	//创建协程
    sc.schedule(c1);		//以协程的形式入队	

    sc.start(); //开启调度器
    sleep(1);
    sc.stop(); //结束调度器

    printLine("invokeFiber end:");
}

void invokeBoth(){
    bin::Scheduler sc(1, false, "test3");   //将调度器命名为"test"

    sc.schedule(&func1);	//以函数形式入队
    bin::Fiber::ptr c1(new bin::Fiber(&func2));	//创建协程
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
    BIN_LOG_INFO(g_logger) << "test in fiber s_count";
    ::sleep(1); //error: 休眠就段错误，读写锁报错

    static int s_count = 5;
    if(--s_count >= 0){
        //BIN_LOG_INFO(g_logger) << "调度" << s_count;
        bin::Scheduler::GetThis()->schedule(&test_fiber, bin::GetThreadId());
    }
}

int main(int argc, char** argv){
    BIN_LOG_INFO(g_logger) << "main";
    // bin::Scheduler sc(3, false, "test");
    bin::Scheduler sc(1, false, "test");
    sc.start();

    BIN_LOG_INFO(g_logger) << "sleep2" << std::endl;

    BIN_LOG_INFO(g_logger) << "schedule" << std::endl;
    sc.schedule(&test_fiber);

    BIN_LOG_INFO(g_logger) << "stop" << std::endl;
    sc.stop();

    BIN_LOG_INFO(g_logger) << "over";
    return 0;
}
#endif


void test_fiber() {
    //static int s_count = 3;
    static int s_count = 2;
    BIN_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    sleep(1);
    if(--s_count >= 0) {
        bin::Scheduler::GetThis()->schedule(&test_fiber, bin::GetThreadId());
    }
}

int main(int argc, char** argv) {
    BIN_LOG_INFO(g_logger) << "main";
    // bin::Scheduler sc(3, false, "test");
    bin::Scheduler sc(1, false, "test");
    sc.start();

    BIN_LOG_INFO(g_logger) << "sleep2" << std::endl;
    sleep(2);

    BIN_LOG_INFO(g_logger) << "schedule" << std::endl;
    sc.schedule(&test_fiber);
    // sleep(2);

    BIN_LOG_INFO(g_logger) << "stop" << std::endl;
    sc.stop();

    BIN_LOG_INFO(g_logger) << "over";
    return 0;
}

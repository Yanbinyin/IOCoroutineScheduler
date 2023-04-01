
/*定时器封装
    功能： 在间隔一个指定时间后，会自动的往任务队列中添加一个函数/协程，执行我们需要的操作。
        支持一次性定时器触发和循环定时器触发

    使用场景：在特定时间段或者某一段时间内需要执行一些任务等场景
*/

#ifndef __BIN_TIMER_H__
#define __BIN_TIMER_H__

#include <memory>
#include <vector>
#include <set>
#include "thread.h"

namespace bin {

    class TimerManager;

    /*Timer类：不能直接创建其对象，构造函数置为私有属性，将其对象创建交由TimerManager类负责管理
    Timer和TimerManager互相置为友元类。
    方便调用对方的接口以及访问对方的成员变量（主要为了TimerManager的addTimer()函数能够调用Timer的构造函数(private)）
    但是造成两个类的封装性被破坏，两个类的耦合度较高
    */
    class Timer : public std::enable_shared_from_this<Timer>{
    friend class TimerManager;
    public:
        typedef std::shared_ptr<Timer> ptr;
        
        bool cancel();  //取消当前定时器的定时任务
        bool refresh(); //刷新设置定时器的执行时间
        bool reset(uint64_t ms, bool from_now); //重置定时器时间 ms: 定时器执行间隔时间(毫秒); from_now: 是否从当前时间开始计算

    private:
        //power: Timer的构造函数为私有意味着不能显示创建对象  必须由TimerManager来创建
        //ms: 定时器执行间隔时间 recurring: 是否循环执行  manager: 定时器管理器
        Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager);
        Timer(uint64_t next);   //next: 执行的时间戳(毫秒)

    private:
        bool m_recurring = false;           //是否循环定时器
        uint64_t m_ms = 0;                  //执行周期
        uint64_t m_next = 0;                //精确的执行时间
        std::function<void()> m_cb;         //回调函数
        TimerManager* m_manager = nullptr;  //定时器管理器

    private:
        //power: 定时器比较仿函数
        /*小技巧：Timer内部构建仿函数作为set容器比较的函数调用
        目的：由于原本的set容器根据存储对象类型，默认比较存储类型的值。存储指针默认通过指针值排列。
        但是我们不希望是通过比较指针值来排列定时器队列，希望通过比较到期时间来进行排列，故需要重置比较函数。*/
        struct Comparator{
            //比较定时器的智能指针的大小(按执行时间排序)
            bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
        };
    };




    /*定时器管理器：虚基类，管理Timer定时器队列，让其能够有序的执行到期后的定时任务
        让其继承类通过纯虚函数virtual void onTimerInsertedAtFront() = 0去加入相关的其他业务拓展*/
    class TimerManager {
    friend class Timer;
    public:
        typedef RWMutex RWMutexType;
        
        TimerManager();
        virtual ~TimerManager();
        
        //创建定时器并添加到定时器集合(m_timers)中   @param[in] ms 定时器执行间隔时间 @param[in] cb 定时器回调函数 @param[in] recurring 是否循环定时器
        Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);
        //添加条件定时器并添加到定时器集合(m_timers)中   @param[in] ms 定时器执行间隔时间 @param[in] weak_cond 条件 @param[in] recurring 是否循环
        Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring = false);
        
        //核心: 获取到最近一个定时器执行的时间间隔(毫秒)
        uint64_t getNextTimer();
        
        //获取需要执行的定时器的回调函数列表   @param[out] cbs 包含cb的回调函数数组
        void listExpiredCb(std::vector<std::function<void()> >& cbs);
        
        bool hasTimer();    //是否有定时器

    protected:
        virtual void onTimerInsertedAtFront() = 0;  //当有新的定时器插入到定时器的首部,执行该函数
        void addTimer(Timer::ptr val, RWMutexType::WriteLock& lock);    //将定时器添加到管理器中

    private:
        bool detectClockRollover(uint64_t now_ms);  //检测服务器时间是否被调后了
        
    private:
        RWMutexType m_mutex;
        std::set<Timer::ptr, Timer::Comparator> m_timers;   //定时器集合
        bool m_tickled = false;         //是否触发onTimerInsertedAtFront //避免频繁修改的一个ticked标记
        uint64_t m_previouseTime = 0;   //上次执行时间
    };

}

#endif

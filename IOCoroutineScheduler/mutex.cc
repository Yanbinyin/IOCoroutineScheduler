/*
SYNOPSIS
    #include <semaphore.h>
    Link with -pthread.

int sem_wait(sem_t *sem);
DESCRIPTION
    sem_wait()  decrements  (locks)  the  semaphore pointed to by sem.  
    If the semaphore's value is greater than zero, then the decrement proceeds, and the function returns, immediately.  
    If the semaphore currently has the value zero, then the call blocks until either it becomes possible to perform the decrement 
        (i.e., the semaphore value rises above zero), or a signal handler interrupts the call.

sem_post(sem_t* sem);
DESCRIPTION
    sem_post()  increments (unlocks) the semaphore pointed to by sem. 
    If the semaphore's value consequently becomes greater than zero, 
        then another process or thread blocked in a sem_wait(3) call will be woken up and proceed tolock the semaphore.

RETURN VALUE
    returns 0 on success; on error, the value of the semaphore is left unchanged, -1 is returned, and errno is set to indicate the error.
*/

#include "mutex.h"
#include "macro.h"
#include "scheduler.h"

namespace bin {
    //block: 信号量
    Semaphore::Semaphore(uint32_t count){
        if(sem_init(&m_semaphore, 0, count)){
            throw std::logic_error("sem_init error");
        }
    }

    Semaphore::~Semaphore(){
        sem_destroy(&m_semaphore);
    }

    void Semaphore::wait(){
        if(sem_wait(&m_semaphore)){
            throw std::logic_error("sem_wait error");
        }
    }

    void Semaphore::notify(){
        if(sem_post(&m_semaphore)){
            throw std::logic_error("sem_post error");
        }
    }



    //block: 协程信号量
    FiberSemaphore::FiberSemaphore(size_t initial_concurrency)
        :m_concurrency(initial_concurrency){
    }

    FiberSemaphore::~FiberSemaphore(){
        SYLAR_ASSERT(m_waiters.empty());
    }

    bool FiberSemaphore::tryWait(){
        SYLAR_ASSERT(Scheduler::GetThis());
        {
            MutexType::Lock lock(m_mutex);
            if(m_concurrency > 0u){
                --m_concurrency;
                return true;
            }
            return false;
        }
    }

    void FiberSemaphore::wait(){
        SYLAR_ASSERT(Scheduler::GetThis());
        {
            MutexType::Lock lock(m_mutex);
            if(m_concurrency > 0u){
                --m_concurrency;
                return;
            }
            m_waiters.push_back(std::make_pair(Scheduler::GetThis(), Fiber::GetThis()));
        }
        Fiber::YieldToHold();
    }

    void FiberSemaphore::notify(){
        MutexType::Lock lock(m_mutex);
        if(!m_waiters.empty()){
            auto next = m_waiters.front();
            m_waiters.pop_front();
            next.first->schedule(next.second);
        }else{
            ++m_concurrency;
        }
    }

}

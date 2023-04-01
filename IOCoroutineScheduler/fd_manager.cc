#include "fd_manager.h"
#include "hook.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
//#include "config.h"

namespace bin{
    //bin::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    FdCtx::FdCtx(int fd)
        :m_isInit(false)
        ,m_isSocket(false)
        ,m_sysNonblock(false)
        ,m_userNonblock(false)
        ,m_isClosed(false)
        ,m_fd(fd)
        ,m_recvTimeout(-1)
        ,m_sendTimeout(-1){
        init();
    }

    FdCtx::~FdCtx(){
    }

    //power: 文件句柄初始化，如果为socket套接字句柄fd，要将其设置为非阻塞状态
    bool FdCtx::init(){
        //句柄初始化过了 就返回
        if(m_isInit)
            return true;

        m_recvTimeout = -1;
        m_sendTimeout = -1;

        //struct stat 获取当前系统文件句柄的状态
        struct stat fd_stat;
        if(-1 == fstat(m_fd, &fd_stat)){ //libfunc:
            m_isInit = false;
            m_isSocket = false;
            //SYLAR_LOG_ERROR(g_logger) << "FdCtx init(): fstat() error";
        }else{
            m_isInit = true;
            //取出状态位 判断句柄类型
            m_isSocket = S_ISSOCK(fd_stat.st_mode);
        }

        if(m_isSocket){
            int flags = fcntl_f(m_fd, F_GETFL, 0);
            //如果句柄阻塞 要设置为非阻塞
            if(!(flags & O_NONBLOCK)){
                fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
            }
            m_sysNonblock = true;
        }else{
            m_sysNonblock = false;
        }

        m_userNonblock = false;
        m_isClosed = false;
        return m_isInit;
    }

    //power:功能：设置/获取文件句柄上的超时时间，分为读（SO_REVTIMEO）/写（SO_SNDTIMEO）超时类型
    void FdCtx::setTimeout(int type, uint64_t t){
        if(type == SO_RCVTIMEO)
            m_recvTimeout = t;
        else
            m_sendTimeout = t;
    }

    uint64_t FdCtx::getTimeout(int type){
        if(type == SO_RCVTIMEO)
            return m_recvTimeout;
        else
            return m_sendTimeout;
    }



    

    FdManager::FdManager(){
        m_fds.resize(64);
    }

    //功能：获取文件句柄对象，如果文件句柄对象不存在可以选择创建
    FdCtx::ptr FdManager::get(int fd, bool auto_create){
        if(fd == -1)
            return nullptr;
        
        RWMutexType::ReadLock lock(m_mutex);
        //如果fd越界说明容量不够
        if((int)m_fds.size() <= fd){
            if(auto_create == false)    //1、越界 && 不允许自动创建 返回空
                return nullptr;            
        }else{ //fd没有越界
            if(m_fds[fd] || !auto_create) //2、不越界 || 允许自动创建 正常返回
                return m_fds[fd];
        }
        lock.unlock();

        //越界 && 不允许自动创建
        RWMutexType::WriteLock lock2(m_mutex);
        FdCtx::ptr ctx(new FdCtx(fd));
        if(fd >= (int)m_fds.size()){
            m_fds.resize(fd * 1.5);
        }
        m_fds[fd] = ctx;
        return ctx;
    }

    void FdManager::del(int fd){
        RWMutexType::WriteLock lock(m_mutex);
        if((int)m_fds.size() <= fd)
            return;
            
        m_fds[fd].reset();
    }

}

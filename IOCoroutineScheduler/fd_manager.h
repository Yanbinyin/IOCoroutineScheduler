//文件句柄管理类
#ifndef __FD_MANAGER_H__
#define __FD_MANAGER_H__

#include <memory>
#include <vector>
#include "thread.h"
#include "singleton.h"

namespace bin {

    //文件句柄上下文类 管理文件句柄类型(是否socket)是否阻塞,是否关闭,读/写超时时间
    class FdCtx : public std::enable_shared_from_this<FdCtx> {
    public:
        typedef std::shared_ptr<FdCtx> ptr;
        FdCtx(int fd);  //通过文件句柄构造FdCtx
        ~FdCtx();
        
        //常用接口
        bool isInit() const { return m_isInit; }                //是否初始化完成
        bool isSocket() const { return m_isSocket; }            //是否socket
        bool isClose() const { return m_isClosed; }             //是否已关闭
        void setUserNonblock(bool v){ m_userNonblock = v; }     //设置用户主动设置非阻塞(v)
        bool getUserNonblock() const { return m_userNonblock; } //获取是否用户主动设置的非阻塞
        void setSysNonblock(bool v){ m_sysNonblock = v; }       //设置系统非阻塞(v)
        bool getSysNonblock() const { return m_sysNonblock; }   //获取系统非阻塞

        //核心接口
        void setTimeout(int type, uint64_t t);  //设置超时时间(ms) type为SO_RCVTIMEO类型(读超时), SO_SNDTIMEO(写超时) t 时间毫秒
        uint64_t getTimeout(int type);          //获取超时时间(ms) type为SO_RCVTIMEO类型(读超时), SO_SNDTIMEO(写超时)

    private:
        bool init();    //初始化

    private:
        bool m_isInit: 1;       //是否初始化
        bool m_isSocket: 1;     //是否socket
        bool m_sysNonblock: 1;  //是否hook非阻塞
        bool m_userNonblock: 1; //是否用户主动设置非阻塞
        bool m_isClosed: 1;     //是否关闭
        int m_fd;               //文件句柄
        uint64_t m_recvTimeout; //读超时时间毫秒 读（SO_REVTIMEO）超时类型
        uint64_t m_sendTimeout; //写超时时间毫秒 写（SO_SNDTIMEO）超时类型
    };



    //文件句柄管理类
    //功能：获取/设置fd属性的操作属于"读多写少"的场景，使用读写锁管理文件句柄的队列。并且将该类置为单例模式
    class FdManager{
    public:
        typedef RWMutex RWMutexType;
        FdManager();
        
        //获取/创建文件句柄类FdCtx，返回对应的指针 fd 文件句柄 auto_create 是否自动创建
        FdCtx::ptr get(int fd, bool auto_create = false);
        
        void del(int fd);   //删除文件句柄类  fd: 文件句柄

    private:
        RWMutexType m_mutex; 
        std::vector<FdCtx::ptr> m_fds;  //文件句柄集合
    };


    typedef Singleton<FdManager> FdMgr; //文件句柄单例
}

#endif

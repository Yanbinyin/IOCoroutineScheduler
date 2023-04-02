/**
 * @file iomanager.cc
 * @author yinyb (990900296@qq.com)
 * @brief 基于Epoll的IO协程调度器
 *  封装套接字句柄对象+事件对象,方便归纳句柄、事件所具有的属性。句柄带有事件，事件依附
 *  于句柄。只有句柄上才有事件触发。
 *  和HOOK模块的文件句柄类做一个区分，这里的套接字句柄专门针对套接字上的事件做回调管理。
 * @version 1.0
 * @date 2022-04-02
 * @copyright Copyright (c) {2022}
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "iomanager.h"
#include "log.h"
#include "macro.h"

namespace bin {

static bin::Logger::ptr g_logger = BIN_LOG_NAME("system");

// hack: 结构体的作用？？？ 重载 << >>？？？
enum EpollCtlOp {};

static std::ostream &operator<<(std::ostream &os, const EpollCtlOp &op) {
  switch ((int)op) {
#define XX(ctl)                                                                \
  case ctl:                                                                    \
    return os << #ctl;
    XX(EPOLL_CTL_ADD);
    XX(EPOLL_CTL_MOD);
    XX(EPOLL_CTL_DEL);
  default:
    return os << (int)op;
  }
#undef XX
}

static std::ostream &operator<<(std::ostream &os, EPOLL_EVENTS events) {
  if (!events) {
    return os << "0";
  }
  bool first = true;
#define XX(E)                                                                  \
  if (events & E) {                                                            \
    if (!first) {                                                              \
      os << "|";                                                               \
    }                                                                          \
    os << #E;                                                                  \
    first = false;                                                             \
  }
  XX(EPOLLIN);
  XX(EPOLLPRI);
  XX(EPOLLOUT);
  XX(EPOLLRDNORM);
  XX(EPOLLRDBAND);
  XX(EPOLLWRNORM);
  XX(EPOLLWRBAND);
  XX(EPOLLMSG);
  XX(EPOLLERR);
  XX(EPOLLHUP);
  XX(EPOLLRDHUP);
  XX(EPOLLONESHOT);
  XX(EPOLLET);
#undef XX
  return os;
}

IOManager::FdContext::EventContext &
IOManager::FdContext::getContext(IOManager::Event event) {
  switch (event) {
  case IOManager::READ:
    return read;
  case IOManager::WRITE:
    return write;
  default:
    BIN_ASSERT2(false, "getContext");
  }
  // 逻辑错误，无效的参数
  throw std::invalid_argument("getContext invalid event");
}

void IOManager::FdContext::resetContext(EventContext &ctx) {
  ctx.scheduler = nullptr;
  ctx.fiber.reset();
  ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
  // BIN_LOG_INFO(g_logger) << "fd=" << fd
  //     << " triggerEvent event=" << event
  //     << " events=" << events;
  BIN_ASSERT(events & event); // 当前事件中必须包含要触发的事件
  // if(BIN_UNLIKELY(!(event & event))) return;
  events = (Event)(events & ~event); // 把触发后的事件去除掉
  EventContext &ctx = getContext(event);
  if (ctx.cb)
    ctx.scheduler->schedule(&ctx.cb);
  else
    ctx.scheduler->schedule(&ctx.fiber);

  // 语雀 add，和原始的
  //  else if(ctx.coroutine){
  //      ctx.scheduler->schedule(&ctx.coroutine);
  //  }

  ctx.scheduler = nullptr;
}

IOManager::IOManager(size_t threads_size, bool use_caller,
                     const std::string &name)
    : Scheduler(threads_size, use_caller, name) { // power:初始化了scheduler()
  BIN_LOG_DEBUG(g_logger) << "IO调度器构造: IOManager";

  // 创建epoll句柄
  m_epfd = epoll_create(5000);
  BIN_ASSERT(m_epfd > 0);

  // 创建管道句柄 m_tickleFds[0]读管道，1为写管道
  int rt = pipe(m_tickleFds); // rt = 0：success
  BIN_ASSERT(!rt);

  // 初始化epoll事件
  epoll_event event;
  memset(&event, 0, sizeof(epoll_event));

  // 设置为 读事件触发 以及 边缘触发
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = m_tickleFds[0];

  // 设置读管道句柄属性 将读fd 设置为非阻塞
  rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
  BIN_ASSERT(!rt);

  // 将当前的事件添加到epoll中
  rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
  BIN_ASSERT(!rt);

  contextResize(32); // 默认为个事件信息

  start(); // 启动IO调度器
}

IOManager::~IOManager() {
  stop();                // 停止调度器
  close(m_epfd);         // 关闭epoll句柄
  close(m_tickleFds[0]); // 关闭读管道句柄
  close(m_tickleFds[1]); // 关闭写管道句柄

  // 删除事件对象分配的空间
  // power: 不使用智能指针的原因：要把空间释放集中到持有调度器的这个线程中
  for (size_t i = 0; i < m_fdContexts.size(); ++i) {
    if (m_fdContexts[i]) {
      delete m_fdContexts[i];
    }
  }
  BIN_LOG_DEBUG(g_logger) << "IO调度器析构: ~IOManager";
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
  FdContext *fd_ctx = nullptr;

  // 拿到对应的句柄对象，没有就创建
  RWMutexType::ReadLock lock(m_mutex);
  if ((int)m_fdContexts.size() > fd) {
    fd_ctx = m_fdContexts[fd];
    lock.unlock();
  } else { // 事件对象扩容
    lock.unlock();
    RWMutexType::WriteLock lock2(m_mutex);
    BIN_LOG_INFO(g_logger) << "fd不存在并扩容,fd=" << fd;
    contextResize(fd * 1.5);
    fd_ctx = m_fdContexts[fd];
  }

  FdContext::MutexType::Lock lock2(fd_ctx->mutex);
  // 一般情况下，一个句柄不会往上面加相同的事件，之前的事件和要添加的事件是同一种类型的事件
  // 说明至少有两个不同的线程在操纵同一个句柄的同一个方法
  if (BIN_UNLIKELY(fd_ctx->events & event)) {
    BIN_LOG_ERROR(g_logger)
        << "addEvent assert fd=" << fd << " event=" << (EPOLL_EVENTS)event
        << " fd_ctx.event=" << (EPOLL_EVENTS)fd_ctx->events;
    BIN_ASSERT(!(fd_ctx->events & event));
  }

  int op =
      fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD; // 判断事件修改还是新增

  struct epoll_event ev;
  ev.events = EPOLLET | fd_ctx->events |
              event; // EPOLLET:位掩码//EPOLLET + 原来event + 当前的
  ev.data.ptr =
      fd_ctx; // 回调的时候，通过数据字段(data)拿回在哪个fd_ctx上面触发的

  int rt = epoll_ctl(m_epfd, op, fd, &ev); // 将事件添加/修改到epoll，成功返回0
  if (rt) {
    BIN_LOG_ERROR(g_logger)
        << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", " << fd
        << ", " << (EPOLL_EVENTS)ev.events << "):" << rt << " (" << errno
        << ") (" << strerror(errno)
        << ") fd_ctx->events=" << (EPOLL_EVENTS)fd_ctx->events;
    return -1;
  }

  ++m_pendingEventCount; // 待处理事件自增

  fd_ctx->events = (Event)(fd_ctx->events | event); // 将句柄上的事件叠加
  // 构建对应的添加的读/写 事件对象 设置相关信息
  // 要加 读事件就返回 read_event;要加写事件 就返回write_event
  FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
  BIN_ASSERT(!event_ctx.scheduler && !event_ctx.fiber &&
             !event_ctx.cb); // 都要是空的，有值说明有事件

  event_ctx.scheduler = Scheduler::GetThis(); // power:设置调度器

  if (cb) { // 设置回调函数
    event_ctx.cb.swap(cb);
  } else { // 没有设置回调  下一次就继续执行当前协程
    event_ctx.fiber = Fiber::GetThis();
    // 给事件对象绑定协程时候 协程应该是运行的
    BIN_ASSERT2(event_ctx.fiber->getState() == Fiber::EXEC,
                "state=" << event_ctx.fiber->getState());
  }
  return 0;
}

bool IOManager::delEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(m_mutex);

  // 1、句柄对象不存在不用删除
  if ((int)m_fdContexts.size() <= fd)
    return false;

  FdContext *fd_ctx = m_fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fd_ctx->mutex);
  // 2、句柄对象存在，但是句柄上没有对应事件 不用删除
  if (BIN_UNLIKELY(!(fd_ctx->events & event)))
    return false;

  // 3、去掉事件：取反运算 + 与运算 就是去掉该事件event
  Event new_events = (Event)(fd_ctx->events & ~event);

  // 去掉之后看句柄上还是否有剩余的事件  有就修改epoll 没有了就从epoll删除
  int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event ev;
  ev.events = EPOLLET | new_events;
  ev.data.ptr = fd_ctx;

  // 将事件ev添加/修改到epoll
  int rt = epoll_ctl(m_epfd, op, fd, &ev);
  if (rt) {
    BIN_LOG_ERROR(g_logger)
        << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", " << fd
        << ", " << (EPOLL_EVENTS)ev.events << "):" << rt << " (" << errno
        << ") (" << strerror(errno) << ")";
    return false;
  }

  --m_pendingEventCount; // 待处理事件对象自减

  fd_ctx->events = new_events; // 更新句柄上的事件

  // 把fdContext句柄对象中对应的读/写事件对象EventContext拿出来清空
  FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
  fd_ctx->resetContext(event_ctx);

  return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(m_mutex);

  // 1、句柄对象不存在
  if ((int)m_fdContexts.size() <= fd)
    return false;

  FdContext *fd_ctx = m_fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fd_ctx->mutex);
  // 2、句柄对象存在，但是句柄上没有对应事件
  if (BIN_UNLIKELY(!(fd_ctx->events & event)))
    return false;

  // 3、去掉事件：取反运算 + 与运算 就是去掉该事件event
  Event new_events = (Event)(fd_ctx->events & ~event);

  // 去掉之后看句柄上还是否有剩余的事件  有就修改epoll 没有了就从epoll删除
  int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = EPOLLET | new_events;
  epevent.data.ptr = fd_ctx;

  // 将事件ev添加/修改到epoll
  int rt = epoll_ctl(m_epfd, op, fd, &epevent);
  if (rt) {
    BIN_LOG_ERROR(g_logger)
        << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", " << fd
        << ", " << (EPOLL_EVENTS)epevent.events << "):" << rt << " (" << errno
        << ") (" << strerror(errno) << ")";
    return false;
  }

  --m_pendingEventCount;

  fd_ctx->triggerEvent(
      event); // 主动触发句柄上事件绑定的回调函数 重新加入到任务队列里
  return true;
}

bool IOManager::cancelAll(int fd) {
  RWMutexType::ReadLock lock(m_mutex);

  // 1、句柄对象不存在不用删除
  if ((int)m_fdContexts.size() <= fd)
    return false;

  FdContext *fd_ctx = m_fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fd_ctx->mutex);
  // 2、句柄对象存在，但是句柄上没有任何事件 不用删除
  if (!fd_ctx->events)
    return false;

  // 直接从epoll里移除该事件
  int op = EPOLL_CTL_DEL;
  epoll_event ev;
  ev.events = 0;
  ev.data.ptr = fd_ctx;

  // 将事件删除到epoll
  int rt = epoll_ctl(m_epfd, op, fd, &ev);
  if (rt) {
    BIN_LOG_ERROR(g_logger)
        << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", " << fd
        << ", " << (EPOLL_EVENTS)ev.events << "):" << rt << " (" << errno
        << ") (" << strerror(errno) << ")";
    return false;
  }

  if (fd_ctx->events & READ) {
    fd_ctx->triggerEvent(READ); // 事件对象 主动触发读事件对象上的回调
    --m_pendingEventCount;
  }
  if (fd_ctx->events & WRITE) {
    fd_ctx->triggerEvent(WRITE); // 事件对象 主动触发写事件对象上的回调
    --m_pendingEventCount;
  }

  BIN_ASSERT(fd_ctx->events == 0); // 句柄对象上的注册事件应该为NONE = 0
  return true;
}

// power: 基类的指针Scheduler*转换成派生类的指针
IOManager *IOManager::GetThis() {
  return dynamic_cast<IOManager *>(Scheduler::GetThis());
}

/*power:功能：线程唤醒
    通过pipe管道特性，将管道读端加入到epoll的管理中。
    通过往管道写端写入数据（写入什么不重要）就能触发读端。
    假设现在只有那个管道读IO活跃，退出epoll_wait()循环监测后，
        没有其他网络IO需要处理，就会swapOut()切回到run()函数中，去查看任务队列的情况，完成一次线程的"唤醒"。
*/
/*几处用到tickle()地方：
    ● schedule()添加任务，判断队列在本次添加前是否空：空就tickle()；不空不唤醒。
    ●
   run()在轮询任务队列的时候，发现有任务不属于自己执行，并且也没有指定任何线程执行的时候要调用tickle()去唤醒线程执行这些任务
    ● stop()准备关闭调度器时候，要把所有创建的线程唤醒，去让他们退出
*/
void IOManager::tickle() {
  BIN_LOG_INFO(g_logger) << "tickle()";
  if (!hasIdleThreads()) {
    return;
  }
  int rt = write(m_tickleFds[1], "T", 1); // 写1个 T 进去
  BIN_ASSERT(rt == 1);
}

bool IOManager::stopping() { // 没给缺省参数值，因为这个override了基类函数
  uint64_t timeout = 0;
  return stopping(timeout);
}

/*功能：
    1.
当前协程分配到的任务完成之后，epoll里检查一下是否有别的IO活动，调度对应的回调函数/协程
    2.
当前协程没有任务可做，IO也没有活动的迹象，陷入到epoll_wait的循环里面，阻塞等待。

while(1){
    1. 检查调度器是否已经停止。已经停止就不能往下执行逻辑，需要退出函数
    2.
陷入epoll_wait，等待IO事件返回，超时时间需要考虑最近快到期的定时任务和默认超时时间取min选其中的小值
    3. 退出epoll_wait之后首先检查，是否是因为有到时定时器需要处理
        a. 有超时定时器，就把所有的超时任务加入调度
        b. 没有就处理就绪IO
    4. 如果有就绪IO，就带回对应IO：
        a. 网络IO，依次轮询触发注册好的异步读/写回调函数作为任务加入队列供调度；
        b.
管道读IO，由tickle()函数发送消息导致的IO活动，用于唤醒，将其忽略（不绑定回调函数作为任务去调度仅仅是唤醒作用）
    5.
如果在超时时间内没有就绪IO/到时的定时器，停止监测，让出当前协程执行权swapOut()，回到Scheduler::run()函数中去
}
*/
void IOManager::idle() {
  BIN_LOG_DEBUG(g_logger) << "IOManager::idle()";
  const uint64_t MAX_EVNETS = 256;
  epoll_event *evts =
      new epoll_event[MAX_EVNETS](); // 256个一组 取出已经就绪的IO

  // power: 借助智能指针的指定析构函数  自动释放数组
  std::shared_ptr<epoll_event> shared_events(
      evts, [](epoll_event *ptr) { delete[] ptr; });

  while (true) {
    uint64_t next_timeout = 0;

    // 1.如果调度器关闭了 就退出该函数
    if (BIN_UNLIKELY(stopping(next_timeout))) {
      BIN_LOG_INFO(g_logger) << "name=" << getName() << ", idle stopping exit";
      break;
    }

    // 2.通过epoll_wait 带回已经就绪的IO
    int rt = 0;
    do {
      static const int MAX_TIMEOUT = 3000; // 最大超时时间
      if (next_timeout != ~0ull) {
        next_timeout =
            (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
      } else {
        next_timeout = MAX_TIMEOUT;
      }
      rt = epoll_wait(m_epfd, evts, MAX_EVNETS, (int)next_timeout);
      if (rt < 0 && errno == EINTR) {
        continue; // 重新尝试等待wait
      } else {
        break; // 拿到了返回epoll_event(rt>0) 或者 已经超时(rt=0)就break
      }
    } while (true);

    // 获取需要执行的定时器的回调函数列表，加入调度器
    std::vector<std::function<void()>> cbs;
    listExpiredCb(cbs);
    if (!cbs.empty()) {
      // BIN_LOG_DEBUG(g_logger) << "on timer cbs.size=" << cbs.size();
      schedule(cbs.begin(), cbs.end());
      cbs.clear();
    }

    // if(BIN_UNLIKELY(rt == MAX_EVNETS)){
    //     BIN_LOG_INFO(g_logger) << "epoll wait events=" << rt;
    // }

    // 3.依次处理已经就绪的IO
    for (int i = 0; i < rt; ++i) {
      epoll_event &ev = evts[i];

      // 外部发消息唤醒的IO，没有实际意义，过滤跳过
      if (ev.data.fd == m_tickleFds[0]) {
        uint8_t dummy[256];
        // 循环的目的，把缓冲区全部读干净，EPOLLET类型不读干净不会再通知
        while (read(m_tickleFds[0], dummy, sizeof(dummy)) > 0)
          ;
        continue;
      }

      // 处理剩下的真正就绪的IO
      // addEvent()的时候把FdContext* fd_ctx添加给data.ptr了
      FdContext *fd_ctx = (FdContext *)ev.data.ptr;
      FdContext::MutexType::Lock lock(fd_ctx->mutex);

      // 事件是epoll_event事件，要分类

      // 如果是错误或者中断 导致的活动  重置一下
      if (ev.events & (EPOLLERR | EPOLLHUP)) {
        BIN_LOG_INFO(g_logger) << "EPOLLERR | EPOLLHUP";
        ev.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
      }

      // 开一个变量转换 从epoll_event的事件----->自定义的事件Event
      int real_events = NONE;
      if (ev.events & EPOLLIN) {
        real_events |= READ;
      }
      if (ev.events & EPOLLOUT) {
        real_events |= WRITE;
      }

      // 和当前IO上的事件对比 为0x0没有事件触发 跳过
      if ((fd_ctx->events & real_events) == NONE) {
        continue;
      }

      // 把下面准备主动触发的事件 去除掉 剩余的事件放回epoll中
      // 因为不知道是单有读 单有写  读写都有 需要涵盖这三种情况
      int left_events =
          (fd_ctx->events & ~real_events); // 剩余事件：当前事件 - 触发事件
      int op = left_events ? EPOLL_CTL_MOD
                           : EPOLL_CTL_DEL; // 有事件：修改 无事件：删除
      ev.events =
          EPOLLET | left_events; // 复用event，ET模式 + 剩余事件后添加到epoll中

      int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &ev);
      if (rt2) {
        BIN_LOG_ERROR(g_logger)
            << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", "
            << fd_ctx->fd << ", " << (EPOLL_EVENTS)ev.events << "):" << rt2
            << " (" << errno << ") (" << strerror(errno) << ")";
        continue;
      }

      // BIN_LOG_INFO(g_logger) << " fd=" << fd_ctx->fd << " events=" <<
      // fd_ctx->events
      //                          << " real_events=" << real_events;

      // 把剩余没有触发的读写事件 主动触发
      if (real_events & READ) {
        BIN_LOG_INFO(g_logger) << "idle 读事件触发";
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
      }
      if (real_events & WRITE) {
        BIN_LOG_INFO(g_logger) << "idle 写事件触发";
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
      }
    }

    // 4.处理完就绪的IO  让出当前协程的执行权 到Scheduler::run中去
    Fiber::ptr cur = Fiber::GetThis();
    auto raw_ptr = cur.get();
    cur.reset();

    raw_ptr->swapOut();
  }
}

// 定时器队列队头插入对象后进行epoll_wait超时更新
void IOManager::onTimerInsertedAtFront() {
  // 唤醒一下 在epoll_wait的线程
  tickle();
}

// 保护类型接口，修改句柄对象数量
void IOManager::contextResize(size_t size) {
  m_fdContexts.resize(size);
  for (size_t i = 0; i < m_fdContexts.size(); ++i) {
    if (!m_fdContexts[i]) {
      m_fdContexts[i] = new FdContext;
      m_fdContexts[i]->fd = i;
    }
  }
}

// 基类stopping() +
// 多判断一下待处理事件数量m_pendingEventCount和定时器队列是否空isTimersEmpty()
bool IOManager::stopping(uint64_t &timeout) {
  timeout = getNextTimer();
  return (timeout == ~0ull) && (m_pendingEventCount == 0) &&
         Scheduler::stopping();
}

} // namespace bin

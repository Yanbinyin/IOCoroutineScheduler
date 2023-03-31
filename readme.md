# IO Coroutine Scheduler

- Design thread pool and coroutine queue, realize asymmetric coroutine and coroutine switch based on ucontext_t, and realize coroutine scheduler.
- Implements the timing function based on epoll timeout, encapsulates all file descriptors of the scheduler, and solves the problem of high CPU usage caused by busy waiting in idle state.
- hook System call interface, enabling apis that do not have asynchronous functions (such as API at the bottom of the system, Socket, IO, and sleep-related apis) to display asynchronous functions.
- Encapsulates epoll and designs I/O coroutine scheduler to achieve high concurrency and high efficiency coroutine scheduling.

<strong>Implementation</strong>  
All codes were wrote in _C++11:_   
moudles you may need:  

_C++:_  
- `cmake` for build repo  
- `boost`   
- `yaml` for setting configuration 

<strong>Usage</strong>  
for C++, you need to compile first  
    `mkdir build && cd build`  
    `cmake ..`  
    `make`  

when it's done, you are ready to run the executable file cmd operations, such as   
    `./bin/test_coroutine`  

<strong>Have fun!</strong>  

## Contents
* Log  
It is an log class that can help you record running information or debug through files or command line.  

* Config  
Set convention priority configuration. Configuration can be modified through code or reading from files.

* Semaphore & Lock  
Protect resources.  

* Thread  
Provides thread classes and thread synchronization classes, based on the `pthread` implementation.
    > Why not use the `thread` provided by C++11?  
    > Because `thread` is actually implemented based on `pthread`. In addition, C++11 does not provide read/write mutex, RWMutex, Spinlock, etc. In high-concurrency scenarios, these objects are often needed, and we do not need cross-platform development (only linux is supported). Therefore, we choose to encapsulate `pthread` by ourselves.

* Corountine  
Realization of asymmetric coroutine based on `ucontext_t`. Coroutine scheduling is not involved.  

* Corountine Scheduling  
A N-M coroutine scheduler is implemented, N threads run M coroutines, coroutines can be switched between threads, can also be bound to the specified thread run.

* IO Corountine Scheduling  
It inherits from the coroutine scheduler, encapsulates epoll, and supports registering read and write event callbacks for socket fd.  
IO coroutine scheduling also solves the problem of high CPU usage caused by the scheduler's busy waiting in idle state. The IO coroutine scheduler uses a pair of pipe FDS to tickle the coroutine. When the scheduler is idle, the idle coroutine blocks on the pipe's read descriptor by epoll_wait, waiting for the pipe's readable event. When a new task is added, the tickle method writes the pipeline, the idle coroutine detects that the pipeline is readable and exits, and the scheduler executes the scheduling. 

* Hook  
hook system base and socket related API, socket IO related API, and sleep series API. The opening control of hooks is thread-granular and optional. Through hook module, some apis without asynchronous functions can be made to show asynchronous performance, such as MySQL.
    > Note: this article refers to the system call interfaces provided by the C standard function library, not to the system calls provided by Linux alone. For example, malloc and free are not system calls; they are the interfaces provided by the C standard function library.  

* Timer  
Implement timer function based on epoll timeout, precision millisecond level, support the execution of callback function after the specified timeout period..  

* Data Processing  
Byte array container that provides serialization and deserialization of the underlying types..  

* Test  
Testcases.  
  
* a lot to be continued...

## Folder structure

+ bin：Binary files
+ build：Build filess
+ cmake：Cmake include files
+ CMakeLists.txt：cmake
+ lib：The output path of the library
+ server-bin：Source code
+ tests：Testing code

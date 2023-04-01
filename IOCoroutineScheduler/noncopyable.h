#ifndef __SYLAR_NONCOPYABLE_H__
#define __SYLAR_NONCOPYABLE_H__

namespace bin {

    //对象无法拷贝,赋值
    class Noncopyable {
    public:
        Noncopyable() = default;
        ~Noncopyable() = default;
        
        Noncopyable(const Noncopyable&) = delete;
        Noncopyable& operator=(const Noncopyable&) = delete;
    };

}

#endif

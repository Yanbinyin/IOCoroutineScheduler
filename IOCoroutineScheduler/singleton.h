#ifndef __BIN_SINGLETON_H__
#define __BIN_SINGLETON_H__

/*
    单例模式：
        保证一个类仅有一个实例，并提供一个访问它的全局访问点，该实例被所有程序模块共享。有很多地方需要这样的功能模块，如系统的日志输出等。
*/

namespace bin{
    template<class T, class X = void, int N = 0>
    class Singleton{
    public:
        static T* GetInstance(){
            static T v;
            return &v;
        }
    };

    template<class T, class X = void, int N = 0>
    class SingletonPtr{
    public:
        static std::shared_ptr<T> GetInstance(){
            static std::shared_ptr<T> v(new T);
            return v;
        }
    };

}

#endif
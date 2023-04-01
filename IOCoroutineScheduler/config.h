#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__

#include <memory>
#include <string>
#include <sstream>                //序列化
#include <boost/lexical_cast.hpp> //power:内存转换
#include <yaml-cpp/yaml.h>
#include "../IOCoroutineScheduler/log.h"
#include <vector>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <iostream> //为了调试config.cc添加，可以删掉

#include "thread.h"
#include "log.h"
#include "util.h"

namespace bin{

    //接口类
    class ConfigVarBase{
    public:
        typedef std::shared_ptr<ConfigVarBase> ptr;
        
        //构造函数：将 配置项名称name 中的字母限定在小写范围内
        ConfigVarBase(const std::string& name, const std::string& description = "")
                    :m_name(name), m_description(description){
            //将m_name转化成小写 
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
        }
        virtual ~ConfigVarBase(){}

        const std::string& getName() const { return m_name; }
        const std::string& getDescription() const { return m_description; }
        
        //将任意输入内容转换为string类型
        virtual std::string toString() = 0;
        //从string类型转换为其他所需类型
        virtual bool fromString(const std::string& val) = 0;
        virtual std::string getTypeName() const = 0;
    protected:
        std::string m_name;         //配置项名称
        std::string m_description;  //配置项描述
    };



    //power:仿函数，函数对象，模板偏特化
    //F from_type, T to_type
    template<class F, class T>
    class LexicalCast{
    public:
        T operator() (const F& v){
            return boost::lexical_cast<T>(v);
        }
    };
    
    //string 和 vector的相互转换
    //从 string 到 vector
    template<class T>
    class LexicalCast<std::string, std::vector<T> >{
    public:
        std::vector<T> operator()(const std::string& v){
            YAML::Node node = YAML::Load(v);
            typename std::vector<T> vec;
            std::stringstream ss;
            for(size_t i = 0; i < node.size(); ++i){
                ss.str("");
                ss << node[i];
                vec.push_back(LexicalCast<std::string, T>()(ss.str()));    //递归调用本身
            }
            return vec;
        }
    };
    //从 vector 到 string
    template<class T>
    class LexicalCast<std::vector<T>, std::string >{
    public:
        std::string operator()(const std::vector<T>& v){
            YAML::Node node;
            for(auto& i : v){
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    //string 和 list的相互转换
    template<class T>
    class LexicalCast<std::string, std::list<T> >{
    public:
        std::list<T> operator()(const std::string& v){
            YAML::Node node = YAML::Load(v);
            typename std::list<T> vec;
            std::stringstream ss;
            for(size_t i = 0; i < node.size(); ++i){
                ss.str("");
                ss << node[i];
                vec.push_back(LexicalCast<std::string, T>()(ss.str()));    //递归调用本身
            }
            return vec;
        }
    };

    template<class T>
    class LexicalCast<std::list<T>, std::string >{
    public:
        std::string operator()(const std::list<T>& v){
            YAML::Node node;
            for(auto& i : v){
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    //string 和 set的相互转换
    template<class T>
    class LexicalCast<std::string, std::set<T> >{
    public:
        std::set<T> operator()(const std::string& v){
            YAML::Node node = YAML::Load(v);
            typename std::set<T> vec;
            std::stringstream ss;
            for(size_t i = 0; i < node.size(); ++i){
                ss.str("");
                ss << node[i];
                vec.insert(LexicalCast<std::string, T>()(ss.str()));    //递归调用本身
            }
            return vec;
        }
    };

    template<class T>
    class LexicalCast<std::set<T>, std::string >{
    public:
        std::string operator()(const std::set<T>& v){
            YAML::Node node;
            for(auto& i : v){
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    //string 和 unordered_set的相互转换
    template<class T>
    class LexicalCast<std::string, std::unordered_set<T> >{
    public:
        std::unordered_set<T> operator()(const std::string& v){
            YAML::Node node = YAML::Load(v);
            typename std::unordered_set<T> vec;
            std::stringstream ss;
            for(size_t i = 0; i < node.size(); ++i){
                ss.str("");
                ss << node[i];
                vec.insert(LexicalCast<std::string, T>()(ss.str()));    //递归调用本身
            }
            return vec;
        }
    };

    template<class T>
    class LexicalCast<std::unordered_set<T>, std::string >{
    public:
        std::string operator()(const std::unordered_set<T>& v){
            YAML::Node node;
            for(auto& i : v){
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // //string 和 map的相互转换
    template<class T>
    class LexicalCast<std::string, std::map<std::string, T> > {
    public:
        std::map<std::string, T> operator()(const std::string& v){
            YAML::Node node = YAML::Load(v);
            typename std::map<std::string, T> vec;
            std::stringstream ss;
            for(auto it = node.begin();
                    it != node.end(); ++it){
                ss.str("");
                ss << it->second;
                vec.insert(std::make_pair(it->first.Scalar(),
                            LexicalCast<std::string, T>()(ss.str())));
            }
            return vec;
        }
    };

    template<class T>
    class LexicalCast<std::map<std::string, T>, std::string> {
    public:
        std::string operator()(const std::map<std::string, T>& v){
            YAML::Node node(YAML::NodeType::Map);
            for(auto& i : v){
                node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    //string 和 unordered_map的相互转换
    template<class T>
    class LexicalCast<std::string, std::unordered_map<std::string, T> > {
    public:
        std::unordered_map<std::string, T> operator()(const std::string& v){
            YAML::Node node = YAML::Load(v);
            typename std::unordered_map<std::string, T> vec;
            std::stringstream ss;
            for(auto it = node.begin();
                    it != node.end(); ++it){
                ss.str("");
                ss << it->second;
                vec.insert(std::make_pair(it->first.Scalar(),
                            LexicalCast<std::string, T>()(ss.str())));
            }
            return vec;
        }
    };

    template<class T>
    class LexicalCast<std::unordered_map<std::string, T>, std::string> {
    public:
        std::string operator()(const std::unordered_map<std::string, T>& v){
            YAML::Node node(YAML::NodeType::Map);
            for(auto& i : v){
                node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };



    //模板子类，继承非模板父类ConfigVarBase
    /*目的：1、实现ConfigVarBase类的接口，并且置为模板类，方便自由的传入参数类型，而后调用 
           2、中封装中好的万能转换去完成各种类型的序列化和反序列化，能使得配置项的存储和落盘较为便捷*/
    /**
     * @brief 配置参数模板子类,保存对应类型的参数值
     * @details T 参数的具体类型
     *          FromStr 从std::string转换成T类型的仿函数
     *          ToStr 从T转换成std::string的仿函数
     *          std::string 为YAML格式的字符串
     */
    //FromStr T operator()(const std::string&)
    //ToStr std::string operator()(const T&)
    template<class T, class FromStr = LexicalCast<std::string, T>, class ToStr = LexicalCast<T, std::string> >
    class ConfigVar: public ConfigVarBase{
    public:
        typedef RWMutex RWMutexType;
        typedef std::shared_ptr<ConfigVar> ptr;
        typedef std::function<void (const T& old_value, const T& new_value)> on_change_cb;

        ConfigVar(const std::string& name, const T& default_val, const std::string& description = "")
            :ConfigVarBase(name, description), m_val(default_val){

        }
        
        //toString():将复杂类型(vector map Person...)转换为string类型，方便处理和输出
        //getValue():获取类型,对于基础类型(int,float...)，不需要toString()处理，可以直接输出
        std::string toString() override {
            try{
                //完成各种类型向string类型的转化
                return ToStr()(m_val);
            } catch (std::exception& e){
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception"
                    << e.what() << " convert: " << typeid(m_val).name() << " to string";
            }
            return "";
        }
        //从string类型转换为其他所需类型
        bool fromString(const std::string& val) override {
            try{
                //完成各种类型向T类型的转化
                setValue(FromStr()(val));
            } catch (std::exception& e){
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::fromString exception "
                    << e.what() << " convert: string to " << typeid(m_val).name() << val << ";";
            }
            return false;
        }

        std::string getTypeName() const override { return typeid(T).name(); }

        const T getValue() const { return m_val; }
        
        //核心函数
        void setValue(const T& v){            
            if(v == m_val)
                return;
            for(auto& i : m_cbs)
                i.second(m_val, v); //实现了对函数对象的调用
            m_val = v;    
        }        
        
        // //配置事件变更old版本
        // void addListener(uint64_t key, on_change_cb cb){
        //     m_cbs[key] = cb;
        // }
        /**
         * @brief 添加变化回调函数
         * @return 返回该回调函数对应的唯一id,用于删除回调
         */
        uint64_t addListener(on_change_cb cb){
            static uint64_t s_fun_id = 0;
            RWMutexType::WriteLock lock(m_mutex);
            ++s_fun_id;
            m_cbs[s_fun_id] = cb;
            return s_fun_id;
        }

        
        void delListener(uint64_t key){
            RWMutexType::WriteLock lock(m_mutex);
            m_cbs.erase(key);
        }
        
        on_change_cb getListener(uint64_t key){
            RWMutexType::ReadLock lock(m_mutex);
            auto it = m_cbs.find(key);
            return it == m_cbs.end() ? nullptr : it->second;
        }
        
        void clearListener(){
            RWMutexType::WriteLock lock(m_mutex);
            m_cbs.clear();
        }

    private:
        T m_val;    //配置项数据
        std::map<uint64_t, on_change_cb> m_cbs; // 配置项所绑定的回调函数容器 uint64_t key要求唯一，一般可以用哈希
        RWMutexType m_mutex;  // 读写锁
    };

    

    //对所有的配置项进行统一管理，更加快捷的创建、访问配置项
    /*  每个配置项都是类模板ConfigVar实例化的一个类的对象
        每个对象的name和ptr都存在Config的s_datas(map)中
    */
    class Config{
    public:
        typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;

        //核心函数模板:查询配置项，不存在就新建一个配置项
        template<class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name){
            auto it = GetDatas().find(name);
            //没找到
            if(it == GetDatas().end()) 
                return nullptr;
            //找到了，s_data的second储存的是基类指针，还要转化成派生类指针            
            return std::dynamic_pointer_cast<ConfigVar<T> > (it->second); //power:与dynamic_cast的区别
        }
        //重载
        template<class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name, const T& default_value, const std::string& description = ""){
            auto it = GetDatas().find(name);
            //if(true): 查找成功，存在，转换成派生类指针
            if(it != GetDatas().end()){
                auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
                //转换成功
                if(tmp){
                    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
                    return tmp;
                }
                //转换失败
                else{
                    SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists but type not " << typeid(T).name() << " real_type = " << it->second->getTypeName() << " " << it->second->toString();
                    return nullptr;
                }
            }
            //if(false): 不存在，创建
            //在name中查找第一个不在 “ab……789” 中的字符的位置
            if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos){
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
                throw std::invalid_argument(name);
            }
            typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
            GetDatas()[name] = v;
            return v;
        }

        static void LoadFromYaml(const YAML::Node& root);
        static ConfigVarBase::ptr LookupBase(const std::string& name);//返回配置项的基类指针
    private:
        //power:static变量未规定初始化顺序，Lookup拿变量的时候可能发现s_datas还未初始化
        static ConfigVarMap& GetDatas(){
            static ConfigVarMap s_datas; //map存储所有的配置项的<name, value>部分，<std::string, ConfigVarBase::ptr>类型
            return s_datas;
        }
    };
}

#endif
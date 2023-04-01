#include "../IOCoroutineScheduler/config.h"
#include "../IOCoroutineScheduler/log.h"
#include <yaml-cpp/yaml.h>
#include <iostream>

#if 0
/*约定优于配置：
        防止以后改变，正常情况下不需要有配置也能正常运行；
        有的时候确实需要修改，通过加个配置的情况下修改它（后面是以加载yaml文件的形式修改的）；
        正常情况下，系统配置几百个，真正需要调整的不多，设置优于配置减少很多工作量
*/
//基本数据类型
bin::ConfigVar<int>::ptr g_int_value_config = bin::Config::Lookup("system.port", (int)8080, "system port");
//检验Config Lookup函数tmp不存在时的报错
bin::ConfigVar<float>::ptr g_int_valuex_config = bin::Config::Lookup("system.port", (float)8080, "system port");    //检测 ConfigVar<T>::ptr Lookup（……）函数
bin::ConfigVar<float>::ptr g_float_value_config = bin::Config::Lookup("system.value", (float)10.2f, "system value");
//STL容器
bin::ConfigVar<std::vector<int> >::ptr g_int_vec_value_config = bin::Config::Lookup("system.int_vec", std::vector<int>{1,2}, "system int vec");
bin::ConfigVar<std::list<int> >::ptr g_int_list_value_config = bin::Config::Lookup("system.int_list", std::list<int>{1,2}, "system int list");
bin::ConfigVar<std::set<int> >::ptr g_int_set_value_config = bin::Config::Lookup("system.int_set", std::set<int>{1,2}, "system int set");
bin::ConfigVar<std::unordered_set<int> >::ptr g_int_uset_value_config = bin::Config::Lookup("system.int_uset", std::unordered_set<int>{1,2}, "system int uset");
bin::ConfigVar<std::map<std::string, int> >::ptr g_str_int_map_value_config = bin::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"k", 2}}, "system str int map");
bin::ConfigVar<std::unordered_map<std::string, int> >::ptr g_str_int_umap_value_config = bin::Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"k", 2}}, "system str int umap");


void print_yaml(const YAML::Node& node, int level){
    if(node.IsScalar()){
        BIN_LOG_INFO(BIN_LOG_ROOT()) << std::string(level * 4, ' ') << node.Scalar() << " - " << node.Type() << " - " << level;
    }else if(node.IsNull()){
        BIN_LOG_INFO(BIN_LOG_ROOT()) << std::string(level * 4, ' ') << "NULL - " << node.Type() << "-" << level;
    }else if(node.IsMap()){
        for(auto it = node.begin(); it != node.end(); ++it){
            BIN_LOG_INFO(BIN_LOG_ROOT()) << std::string(level * 4, ' ') << it->first << " - " << it->second.Type() << " - " << level;
            print_yaml(it->second, level + 1);
        }
    }else if(node.IsSequence()){
        for(size_t i = 0; i < node.size(); ++i){
            BIN_LOG_INFO(BIN_LOG_ROOT()) << std::string(level * 4, ' ') << i << " - " << node[i].Type() << " - " << level;
            print_yaml(node[i], level);
        }
    }
}

//加载yaml
void test_yaml(){
    //YAML::Node root = YAML::LoadFile("../bin/conf/log.yml" );
    YAML::Node root = YAML::LoadFile("../bin/conf/test.yml" );
    print_yaml(root, 0);
    BIN_LOG_INFO(BIN_LOG_ROOT()) << root;
}

//测试基本数据类型和STL
void test_config(){
    BIN_LOG_INFO(BIN_LOG_ROOT()) << "before: " << g_int_value_config->getValue();
    BIN_LOG_INFO(BIN_LOG_ROOT()) << "before:" << g_float_value_config->toString();

//vector list set...容器专用宏
#define XX(g_var, name, prefix) \
    { \
        auto& v = g_var->getValue(); \
        for(auto& i : v){ \
            BIN_LOG_INFO(BIN_LOG_ROOT()) << #prefix ": " #name ":" << i; \
        } \
        BIN_LOG_INFO(BIN_LOG_ROOT()) << #prefix ": " #name " yaml :" << g_var->toString(); \
    }
//map专用宏
#define XX_M(g_var, name, prefix) \
    { \
        auto& v = g_var->getValue(); \
        for(auto& i : v){ \
            BIN_LOG_INFO(BIN_LOG_ROOT()) << #prefix ": " #name ": {" << i.first << " - " << i.second << "}"; \
        } \
        BIN_LOG_INFO(BIN_LOG_ROOT()) << #prefix ": " #name " yaml :" << g_var->toString(); \
    }
    XX(g_int_vec_value_config, int_vec, before);
    XX(g_int_list_value_config, int_list, before);
    XX(g_int_set_value_config, int_set, before);
    XX(g_int_uset_value_config, int_uset, before);
    XX_M(g_str_int_map_value_config, str_int_map, before);
    XX_M(g_str_int_umap_value_config, str_int_umap, before);


    //YAML::Node root = YAML::LoadFile("../bin/conf/log.yml" );
    YAML::Node root = YAML::LoadFile("../bin/conf/test.yml" );
    bin::Config::LoadFromYaml(root);
    
    BIN_LOG_INFO(BIN_LOG_ROOT()) << "after:" << g_int_value_config->getValue();
    BIN_LOG_INFO(BIN_LOG_ROOT()) << "after:" << g_float_value_config->toString();

    XX(g_int_vec_value_config, int_vec, after);
    XX(g_int_list_value_config, int_list, after);
    XX(g_int_set_value_config, int_set, after);
    XX(g_int_uset_value_config, int_uset, after);
    XX_M(g_str_int_map_value_config, str_int_map, after);
    XX_M(g_str_int_umap_value_config, str_int_map, after);
}
    
#endif



//测试自定义数据类型
#if 0

class Person {
public:
    Person(){};
    std::string m_name;
    int m_age = 0;
    //bool m_sex = false;
    std::string m_sex;

    std::string toString() const {
        std::stringstream ss;
        ss << "[Person name=" << m_name << " age=" << m_age << " sex=" << m_sex << "]";
        return ss.str();
    }

    bool operator == (const Person& oth) const {
        return m_name == oth.m_name && m_age == oth.m_age && m_sex == oth.m_sex;
    }
};


namespace bin{
    template<>
    class LexicalCast<std::string, Person>{
    public:
        Person operator()(const std::string& v){
            /*  v:yml文件多行组成的string 
                node:字符序列,类似map*/
            YAML::Node node = YAML::Load(v);
            Person p; 
            //std::cout << "转换之前:  " << "v: " << v << "" << p.m_name << " " << p.m_age << " " << p.m_sex << std::endl;
            //核心转换         
            p.m_name = node["name"].as<std::string>();
            p.m_age = node["age"].as<int>();    
            //p.m_sex = node["sex"].as<bool>(); //bug:string转bool有问题,视频14没找到原因
            p.m_sex = node["sex"].as<std::string>();
            //std::cout << "转换结果" << "m_name:" << p.m_name << node["name"] << ", " << "m_age:" << p.m_age << node["age"] << "," << "m_sex:" << p.m_sex << node["sex"] << ". " << std::endl;
            return p;
        }
    };

    template<>
    class LexicalCast<Person, std::string>{
    public:
        std::string operator()(const Person& p){
            YAML::Node node;
            node["name"] = p.m_name;
            node["age"] = p.m_age;
            node["sex"] = p.m_sex;
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
}


//test Person
bin::ConfigVar<Person>::ptr g_person = bin::Config::Lookup("class.person", Person(), "system person");
//test Map<string, Person>
bin::ConfigVar<std::map<std::string, Person> >::ptr g_person_map = bin::Config::Lookup("class.map", std::map<std::string, Person>(), "system person");
//test map<string, vector<Person> > >
bin::ConfigVar<std::map<std::string, std::vector<Person> > >::ptr g_person_vec_map = bin::Config::Lookup("class.vec_map", std::map<std::string, std::vector<Person>>(), "system person");

void test_class(){

    std::cout << "-----Before:" << std::endl;
    BIN_LOG_INFO(BIN_LOG_ROOT()) << " ---  " << g_person->getValue().toString() << " --- " << g_person->toString();
    
    //XX_PersonMap专用宏
    #define XX_PM(g_var, prefix){ \
        auto m = g_person_map->getValue(); \
        for(auto& i : m){ \
            BIN_LOG_INFO(BIN_LOG_ROOT()) << prefix << ": " << i.first << " - " << i.second.toString(); \
        } \
        BIN_LOG_INFO(BIN_LOG_ROOT()) << prefix << ": size=" << m.size(); \
    }
    
    //g_person->addListener(10, [] (const Person& old_value, const Person& new_value){BIN_LOG_INFO(BIN_LOG_ROOT()) << "old_value=" << old_value.toString() << " new_value=" << new_value.toString();});
    g_person->addListener([] (const Person& old_value, const Person& new_value){BIN_LOG_INFO(BIN_LOG_ROOT()) << "old_value=" << old_value.toString() << " new_value=" << new_value.toString();});

    XX_PM(g_person_map, "class.map before");
    BIN_LOG_INFO(BIN_LOG_ROOT()) << "class.vec.map before:" << g_person_vec_map->toString();
    
    std::cout << std::endl << "-----ReadFile:" << std::endl;
    YAML::Node root = YAML::LoadFile("../bin/conf/test.yml");
    std::cout << "一旦g_person被修改，addListener就会打印变化：" << std::endl; 
    bin::Config::LoadFromYaml(root);

    std::cout << std::endl << "------After:" << std::endl;
    BIN_LOG_INFO(BIN_LOG_ROOT()) << g_person->getValue().toString() << " - " << g_person->toString();
    XX_PM(g_person_map, "class.map after");
    BIN_LOG_INFO(BIN_LOG_ROOT()) << "class.vec.map after:" << g_person_vec_map->toString();
}

#endif



void test_log(){    
    static bin::Logger::ptr system_log = BIN_LOG_NAME("system");
    ////查看system的 level 和 name
    // std::cout << "system level:" << bin::LogLevel::ToString(system_log->getLevel()) << ", system name:" << system_log->getName() << std::endl;
    BIN_LOG_INFO(system_log) << "hello system";
    std::cout << "before: " << std::endl << bin::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    
    YAML::Node root = YAML::LoadFile("../bin/conf/log.yml");
    bin::Config::LoadFromYaml(root);
    std::cout << "====================" << std::endl;
    
    std::cout << "After:" << std::endl << bin::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    std::cout << "====================" << std::endl;
    std::cout << root << std::endl;
    BIN_LOG_INFO(system_log) << "hello system";

    system_log->setFormatter("%d - %m%n");
    BIN_LOG_INFO(system_log) << "hello, system" << std::endl;
}


int main(){
    // test_yaml();
    // std::cout << "--------" << std::endl;
    // test_config();
    // std::cout << "--------" << std::endl;
    // test_class();
    // std::cout << "--------" << std::endl; 
    test_log();
    return 0;
}

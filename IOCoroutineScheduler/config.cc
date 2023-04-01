#include "config.h"

namespace bin{

    //定义static变量
    //Config::ConfigVarMap Config::s_datas;   

    ConfigVarBase::ptr Config::LookupBase(const std::string& name){
        auto it = GetDatas().find(name);
        return it == GetDatas().end() ? nullptr : it->second;
    }

    //辅助函数：将yaml结点进行序列化，把每一个配置项扁平化
    static void ListAllMember(const std::string& prefix, const YAML::Node& node, std::list<std::pair<std::string, const YAML::Node> >& output){
        //包含非法字符，直接返回
        if(prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._012345678 ") != std::string::npos){
            BIN_LOG_ERROR(BIN_LOG_ROOT()) << "Config invalid name: " << prefix << " : " << node;
            return;
        }
        
        //不包含非法字符，压入output
        output.push_back(std::make_pair(prefix, node));

        //如果是Map类型，递归处理
        //因为分层了，如果下一层还是object，继续遍历
        if(node.IsMap()){
            for(auto it = node.begin(); it != node.end(); ++it){
                ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
            }
        }
    }

    //核心函数：从yaml结点中加载配置项数据
    //一般在调用YAML库的接口YAML::LoadFile(文件路径)后从文件中读取数据构造成一个yaml结点，再调用该接口从原始yaml结点中拆解出对应的配置项，依次对需要的配置进行一个设置
    void Config::LoadFromYaml(const YAML::Node& root){
        std::list<std::pair<std::string, const YAML::Node>> all_nodes;
        //传入root，将yaml文件中的所有信息存入all_nodes中
        ListAllMember("", root, all_nodes);
        // std::cout << "root:" << root << std::endl << std::endl;
        // std::cout << "all_nodes: " << std::endl;
        // static int j = 0;
        // for(auto& i : all_nodes){            
        //     ++j;
        //     std::cout << j << " " << i.first << " -- " << i.second << std::endl << std::endl;
        // }
        // std::cout << std::endl << "j = " << j << std::endl;
        // std::cout << "all_nodes end;" << std::endl;
        for(auto& i : all_nodes){
            std::string key = i.first;
            // std::cout << "key = " << key << std::endl;
            if(key.empty()){
                continue;
            }
            
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            //var:s_datas中key(name)所对应的value，即ptr
            //从ConfigVar 寻找是否有已经约定的配置项，如果有就进行一个修改
            ConfigVarBase::ptr var = LookupBase(key);//bin:查询更新
            
            //如果成功返回配置项的基类指针
            if(var){
                if(i.second.IsScalar()){
                    var->fromString(i.second.Scalar());
                }else{
                    std::stringstream ss;
                    ss << i.second;
                    var->fromString(ss.str());  //调用setValue()实现更新并调用lamda表达式输出更改后的信息
                }
            }
        }
        //std::cout << "LoadFromYaml end success" << std::endl;
    }
}
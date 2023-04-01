#ifndef __BIN_MODULE_H__
#define __BIN_MODULE_H__

#include "IOCoroutineScheduler/stream.h"
#include "IOCoroutineScheduler/singleton.h"
#include "IOCoroutineScheduler/mutex.h"
#include "IOCoroutineScheduler/rock/rock_stream.h"
#include <map>
#include <unordered_map>

namespace bin {
/**
 * extern "C" {
 * Module* CreateModule(){
 *  return XX;
 * }
 * void DestoryModule(Module* ptr){
 *  delete ptr;
 * }
 * }
 */
class Module {
public:
    enum Type {
        MODULE = 0,
        ROCK = 1,
    };
    typedef std::shared_ptr<Module> ptr;
    Module(const std::string& name
            ,const std::string& version
            ,const std::string& filename
            ,uint32_t type = MODULE);
    virtual ~Module(){}

    virtual void onBeforeArgsParse(int argc, char** argv);
    virtual void onAfterArgsParse(int argc, char** argv);

    virtual bool onLoad();
    virtual bool onUnload();

    virtual bool onConnect(bin::Stream::ptr stream);
    virtual bool onDisconnect(bin::Stream::ptr stream);
    
    virtual bool onServerReady();
    virtual bool onServerUp();

    virtual bool handleRequest(bin::Message::ptr req
                               ,bin::Message::ptr rsp
                               ,bin::Stream::ptr stream);
    virtual bool handleNotify(bin::Message::ptr notify
                              ,bin::Stream::ptr stream);

    virtual std::string statusString();

    const std::string& getName() const { return m_name;}
    const std::string& getVersion() const { return m_version;}
    const std::string& getFilename() const { return m_filename;}
    const std::string& getId() const { return m_id;}

    void setFilename(const std::string& v){ m_filename = v;}

    uint32_t getType() const { return m_type;}

    void registerService(const std::string& server_type,
            const std::string& domain, const std::string& service);
protected:
    std::string m_name;
    std::string m_version;
    std::string m_filename;
    std::string m_id;
    uint32_t m_type;
};

class RockModule : public Module {
public:
    typedef std::shared_ptr<RockModule> ptr;
    RockModule(const std::string& name
               ,const std::string& version
               ,const std::string& filename);

    virtual bool handleRockRequest(bin::RockRequest::ptr request
                        ,bin::RockResponse::ptr response
                        ,bin::RockStream::ptr stream) = 0;
    virtual bool handleRockNotify(bin::RockNotify::ptr notify
                        ,bin::RockStream::ptr stream) = 0;

    virtual bool handleRequest(bin::Message::ptr req
                               ,bin::Message::ptr rsp
                               ,bin::Stream::ptr stream);
    virtual bool handleNotify(bin::Message::ptr notify
                              ,bin::Stream::ptr stream);

};

class ModuleManager {
public:
    typedef RWMutex RWMutexType;

    ModuleManager();

    void add(Module::ptr m);
    void del(const std::string& name);
    void delAll();

    void init();

    Module::ptr get(const std::string& name);

    void onConnect(Stream::ptr stream);
    void onDisconnect(Stream::ptr stream);

    void listAll(std::vector<Module::ptr>& ms);
    void listByType(uint32_t type, std::vector<Module::ptr>& ms);
    void foreach(uint32_t type, std::function<void(Module::ptr)> cb);
private:
    void initModule(const std::string& path);
private:
    RWMutexType m_mutex;
    std::unordered_map<std::string, Module::ptr> m_modules;
    std::unordered_map<uint32_t
        ,std::unordered_map<std::string, Module::ptr> > m_type2Modules;
};

typedef bin::Singleton<ModuleManager> ModuleMgr;

}

#endif

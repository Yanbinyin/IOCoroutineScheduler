#include "http.h"
#include "server-bin/util.h"


namespace sylar::http {
//namespace http {

    HttpMethod StringToHttpMethod(const std::string& method){
    #define XX(num, name, string) \
        if(strcmp(#string, method.c_str()) == 0){ \
            return HttpMethod::name; \
        }
        HTTP_METHOD_MAP(XX);
    #undef XX
        return HttpMethod::INVALID_METHOD;
    }

    HttpMethod CharsToHttpMethod(const char* method){
    #define XX(num, name, string) \
        if(strncmp(#string, method, strlen(#string)) == 0){ \
            return HttpMethod::name; \
        }
        HTTP_METHOD_MAP(XX);
    #undef XX
        return HttpMethod::INVALID_METHOD;
    }

    //辅助数组，将HTTP请求方法由枚举转为数组方便访问 数字从0开始定义 转为数组实现
    /*s_method_string={"DELETE",  "GET", "HEAD", "POST", …… , "SOURCE",}*/
    static const char* s_method_string[] = {
    #define XX(num, name, string) #string,
        HTTP_METHOD_MAP(XX)
    #undef XX
    };

    const char* HttpMethodToString(const HttpMethod& method){
        uint32_t index = (uint32_t)method;
        if(index >= (sizeof(s_method_string) / sizeof(s_method_string[0]))){
            return "<unknown>";
        }
        return s_method_string[index];
    }

    const char* HttpStatusToString(const HttpStatus& status){
        switch(status){
    #define XX(code, name, msg) \
            case HttpStatus::name: \
                return #msg;
            HTTP_STATUS_MAP(XX);
    #undef XX
            default:
                return "<unknown>";
        }
    }



    bool CaseInsensitiveLess::operator()(const std::string& lhs, const std::string& rhs) const {
        return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
    }


    //block: HttpRequest
    
    HttpRequest::HttpRequest(uint8_t version, bool close)
        :m_method(HttpMethod::GET)
        ,m_version(version)
        ,m_close(close)
        ,m_websocket(false)
        ,m_parserParamFlag(0)
        ,m_path("/"){
    }

    std::shared_ptr<HttpResponse> HttpRequest::createResponse(){
        HttpResponse::ptr rsp(new HttpResponse(getVersion(), isClose()));
        return rsp;
    }

    std::string HttpRequest::getHeader(const std::string& key, const std::string& def) const {
        auto it = m_headers.find(key);
        return it == m_headers.end() ? def : it->second;
    }

    std::string HttpRequest::getParam(const std::string& key, const std::string& def){
        initQueryParam();
        initBodyParam();
        auto it = m_params.find(key);
        return it == m_params.end() ? def : it->second;
    }

    std::string HttpRequest::getCookie(const std::string& key, const std::string& def){
        initCookies();
        auto it = m_cookies.find(key);
        return it == m_cookies.end() ? def : it->second;
    }

    void HttpRequest::setHeader(const std::string& key, const std::string& val){
        m_headers[key] = val;
    }

    void HttpRequest::setParam(const std::string& key, const std::string& val){
        m_params[key] = val;
    }

    void HttpRequest::setCookie(const std::string& key, const std::string& val){
        m_cookies[key] = val;
    }

    void HttpRequest::delHeader(const std::string& key){
        m_headers.erase(key);
    }

    void HttpRequest::delParam(const std::string& key){
        m_params.erase(key);
    }

    void HttpRequest::delCookie(const std::string& key){
        m_cookies.erase(key);
    }

    bool HttpRequest::hasHeader(const std::string& key, std::string* val){
        auto it = m_headers.find(key);
        if(it == m_headers.end()){
            return false;
        }
        if(val){
            *val = it->second;
        }
        return true;
    }

    bool HttpRequest::hasParam(const std::string& key, std::string* val){
        initQueryParam();
        initBodyParam();
        auto it = m_params.find(key);
        if(it == m_params.end()){
            return false;
        }
        if(val){
            *val = it->second;
        }
        return true;
    }

    bool HttpRequest::hasCookie(const std::string& key, std::string* val){
        initCookies();
        auto it = m_cookies.find(key);
        if(it == m_cookies.end()){
            return false;
        }
        if(val){
            *val = it->second;
        }
        return true;
    }

    std::ostream& HttpRequest::dump(std::ostream& os) const {
        //GET /uri HTTP/1.1
        //Host: wwww.sylar.top
        //空行CR+LF
        //实体

         //请求行封装
        os << HttpMethodToString(m_method) << " "
        << m_path
        << (m_query.empty() ? "" : "?")
        << m_query
        << (m_fragment.empty() ? "" : "#")
        << m_fragment
        << " HTTP/"
        << ((uint32_t)(m_version >> 4))
        << "."
        << ((uint32_t)(m_version & 0x0F))
        << "\r\n";

        //连接状态首部字段单独列举
        if(!m_websocket)
            os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
        
        //首部字段封装
        for(auto& i : m_headers){
            if(!m_websocket && strcasecmp(i.first.c_str(), "connection") == 0)
                continue;
            os << i.first << ": " << i.second << "\r\n";
        }

        //报文实体封装
        if(!m_body.empty()){
            os << "content-length: " << m_body.size() << "\r\n\r\n" << m_body;
        }else{
            os << "\r\n";
        }
        return os;
    }

    
    std::string HttpRequest::toString() const {
        std::stringstream ss;
        dump(ss);
        return ss.str();
    }

    void HttpRequest::init(){
        std::string conn = getHeader("connection");
        if(!conn.empty()){
            if(strcasecmp(conn.c_str(), "keep-alive") == 0){
                m_close = false;
            }else{
                m_close = true;
            }
        }
    }

    void HttpRequest::initParam(){
        initQueryParam();
        initBodyParam();
        initCookies();
    }

    void HttpRequest::initQueryParam(){
        if(m_parserParamFlag & 0x1)
            return;

    //parse param：解析参数
    #define PARSE_PARAM(str, mp, flag, trim) \
        size_t pos = 0; \
        do { \
            size_t last = pos; \
            pos = str.find('=', pos); \
            if(pos == std::string::npos)/*string::npos 表示不存在的位置*/ \
                break; \
            size_t key = pos; \
            pos = str.find(flag, pos); \
            mp.insert(std::make_pair(trim(str.substr(last, key - last)), \
                        sylar::StringUtil::UrlDecode(str.substr(key + 1, pos - key - 1)))); \
            if(pos == std::string::npos) \
                break; \
            ++pos; \
        } while(true);

        PARSE_PARAM(m_query, m_params, '&',);
        m_parserParamFlag |= 0x1;
    }

    void HttpRequest::initBodyParam(){
        if(m_parserParamFlag & 0x2)
            return;
        std::string content_type = getHeader("content-type");
        if(strcasestr(content_type.c_str(), "application/x-www-form-urlencoded") == nullptr){
            m_parserParamFlag |= 0x2;
            return;
        }
        PARSE_PARAM(m_body, m_params, '&',);
        m_parserParamFlag |= 0x2;
    }

    void HttpRequest::initCookies(){
        if(m_parserParamFlag & 0x4)
            return;
        std::string cookie = getHeader("cookie");
        if(cookie.empty()){
            m_parserParamFlag |= 0x4;
            return;
        }
        PARSE_PARAM(cookie, m_cookies, ';', sylar::StringUtil::Trim);
        m_parserParamFlag |= 0x4;
    }



    //block: HttpResponse

    HttpResponse::HttpResponse(uint8_t version, bool close)
        :m_status(HttpStatus::OK)
        ,m_version(version)
        ,m_close(close)
        ,m_websocket(false){
    }

    std::string HttpResponse::getHeader(const std::string& key, const std::string& def) const {
        auto it = m_headers.find(key);
        return it == m_headers.end() ? def : it->second;
    }

    void HttpResponse::setHeader(const std::string& key, const std::string& val){
        m_headers[key] = val;
    }

    void HttpResponse::delHeader(const std::string& key){
        m_headers.erase(key);
    }

    void HttpResponse::setRedirect(const std::string& uri){
        m_status = HttpStatus::FOUND;
        setHeader("Location", uri);
    }

    void HttpResponse::setCookie(const std::string& key, const std::string& val,
                                time_t expired, const std::string& path,
                                const std::string& domain, bool secure){
        std::stringstream ss;
        ss << key << "=" << val;
        if(expired > 0){
            ss << ";expires=" << sylar::Time2Str(expired, "%a, %d %b %Y %H:%M:%S") << " GMT";
        }
        if(!domain.empty()){
            ss << ";domain=" << domain;
        }
        if(!path.empty()){
            ss << ";path=" << path;
        }
        if(secure){
            ss << ";secure";
        }
        m_cookies.push_back(ss.str());
    }


    std::string HttpResponse::toString() const {
        std::stringstream ss;
        dump(ss);
        return ss.str();
    }

    std::ostream& HttpResponse::dump(std::ostream& os) const {
        os << "HTTP/"
        << ((uint32_t)(m_version >> 4))
        << "."
        << ((uint32_t)(m_version & 0x0F))
        << " "
        << (uint32_t)m_status
        << " "
        << (m_reason.empty() ? HttpStatusToString(m_status) : m_reason)
        << "\r\n";

        for(auto& i : m_headers){
            if(!m_websocket && strcasecmp(i.first.c_str(), "connection") == 0)
                continue;
            os << i.first << ": " << i.second << "\r\n";
        }
        for(auto& i : m_cookies){
            os << "Set-Cookie: " << i << "\r\n";
        }
        if(!m_websocket){
            os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
        }
        if(!m_body.empty()){
            os << "content-length: " << m_body.size() << "\r\n\r\n" << m_body;
        }else{
            os << "\r\n";
        }
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const HttpRequest& req){
        return req.dump(os);
    }

    std::ostream& operator<<(std::ostream& os, const HttpResponse& rsp){
        return rsp.dump(os);
    }

//}
}

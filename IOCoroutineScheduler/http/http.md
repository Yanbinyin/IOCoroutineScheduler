httpServlet是URL路径映射到处理函数上面，也和httpServer封装在了一起，通过URL请求不同的文件实现不同的功能需要实现不同的Servlet类

---

listen并accept的socket用HttpSession来封装

主动请求的socket用HTTPConnection来封装

Server端 receive的是request，send的是response

Client端 receive的是response，send的是request

因此要分开封装


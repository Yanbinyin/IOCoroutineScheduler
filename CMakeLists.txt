# #测试配置系统和日志系统
# cmake_minimum_required(VERSION 2.8)
# project(ServerBin)

# include (cmake/utils.cmake)

# set(CMAKE_VERSION_FILE ON)  #make过程中显示详细命令
# # set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function") # 为了方便调试输出信息，取消优化
# # set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")
# set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -g -O0 -Wall -Wno-builtin-macro-redefined")

# include_directories(.)
# include_directories(/apps/ServerBin/include)   # yaml安装的文件夹，我好像安装在lib里面了，所以不需要
# link_directories(/apps/ServerBin/lib)

# find_library(YAMLCPP yaml-cpp)
# message("***"  ${YAMLCPP})

# set(CMAKE_BUILD_TYPE Debug)

# set(LIB_SRC
#     ServerBin/log.cc
#     ServerBin/util.cc
#     ServerBin/config.cc
#     ServerBin/thread.cc
#     )

# add_library(ServerBin SHARED ${LIB_SRC})    #生成lib
# redefine_file_macro(ServerBin)  #__FILE__
# # add_library(ServerBin_static STATIC ${LIB_SRC})
# # SET_TARGET_PROPERTIES(ServerBin_static PROPERTIONS OUTPUT_NAME "server-bin")

# set(LIB_LIB
#     ServerBin
#     pthread
#     ${YAMLCPP}
#     )

# add_executable(test tests/test.cc)      #生成测试文件
# add_dependencies(test ServerBin)            #测试文件依赖于lib
# redefine_file_macro(test)
# target_link_libraries(test ServerBin ${LIB_LIB})

# add_executable(test_config tests/test_config.cc)
# add_dependencies(test_config ServerBin)
# redefine_file_macro(test_config)
# target_link_libraries(test_config ServerBin ${LIB_LIB})

# set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)




# 测试线程系统
cmake_minimum_required(VERSION 2.8)
project(ServerBin)

include (cmake/utils.cmake)

set(CMAKE_VERSION_FILE ON)  #make过程中显示详细命令
# set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function") # 为了方便调试输出信息，取消优化
# set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -g -O0 -Wall -Wno-builtin-macro-redefined")

include_directories(.)
include_directories(/apps/ServerBin/include)   # yaml安装的文件夹，我好像安装在lib里面了，所以不需要
link_directories(/apps/ServerBin/lib)

set(CMAKE_BUILD_TYPE Debug)

set(LIB_SRC 
    server-bin/address.cc
    server-bin/bytearray.cc
    server-bin/config.cc
    server-bin/coroutine.cc
    server-bin/fd_manager.cc
    server-bin/hook.cc
    server-bin/http/http.cc
    server-bin/http/http_parser.cc
    # server-bin/http/http11_parser.rl.cc
    # server-bin/http/httpclient_parser.rl.cc
    server-bin/http/http_connection.cc
    server-bin/http/http_server.cc
    server-bin/http/http_session.cc
    server-bin/http/servlet.cc
    # server-bin/http/servlet/config_servlet.cc
    # server-bin/http/servlet/status_servlet.cc
    server-bin/iomanager.cc
    server-bin/log.cc
    server-bin/mutex.cc
    server-bin/scheduler.cc
    server-bin/socket.cc
    server-bin/stream.cc
    # server-bin/streams/zlib_stream.cc
    server-bin/streams/socket_stream.cc
    server-bin/thread.cc
    server-bin/timer.cc
    server-bin/tcp_server.cc
    # server-bin/uri.rl.cpp
    server-bin/util.cc
    server-bin/util/crypto_util.cc
    server-bin/util/json_util.cc
    server-bin/util/hash_util.cc
)

ragelmaker(server-bin/http/http11_parser.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/server-bin/http)
ragelmaker(server-bin/http/httpclient_parser.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/server-bin/http)
ragelmaker(server-bin/uri.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/server-bin)

add_library(ServerBin SHARED ${LIB_SRC})    #生成lib
redefine_file_macro(ServerBin)  #__FILE__
# add_library(ServerBin_static STATIC ${LIB_SRC})
# SET_TARGET_PROPERTIES(ServerBin_static PROPERTIONS OUTPUT_NAME "server-bin")


find_library(YAMLCPP yaml-cpp)
find_library(PTHREAD pthread)



set(LIBS
    ServerBin
    dl #编译 dlsym（man dlsym查看）
    pthread
    yaml-cpp
    jsoncpp
    ssl
    protobuf
    )

message("***"  ${LIBS})

# function(serverBin_add_executable targetname srcs depends libs)
#     add_executable(${targetname} ${srcs})
#     add_dependencies(${targetname} ${depends})
#     force_redefine_file_macro_for_sources(${targetname})
#     target_link_libraries(${targetname} ${libs})
# endfunction()
###########  相当于下面的示例  #################
# add_executable(test tests/test.cc)      #生成测试文件
# add_dependencies(test ServerBin)            #测试文件依赖于lib
# redefine_file_macro(test)
# target_link_libraries(test ${LIBS})

serverBin_add_executable(test_http_server "tests/test_http_server.cc" sylar "${LIBS}")
serverBin_add_executable(test_http_connection "tests/test_http_connection.cc" sylar "${LIBS}")
serverBin_add_executable(test_uri "tests/test_uri.cc" sylar "${LIBS}")

add_executable(test tests/test.cc)      #生成测试文件
add_dependencies(test ServerBin)            #测试文件依赖于lib
redefine_file_macro(test)
target_link_libraries(test ${LIBS})

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config ServerBin)
redefine_file_macro(test_config)
target_link_libraries(test_config ${LIBS})

add_executable(test_thread tests/test_thread.cc)
add_dependencies(test_thread ServerBin)
redefine_file_macro(test_thread)
target_link_libraries(test_thread ${LIBS})

add_executable(test_util tests/test_util.cc)
add_dependencies(test_util ServerBin)
redefine_file_macro(test_util)
target_link_libraries(test_util ${LIBS})

add_executable(test_coroutine tests/test_coroutine.cc)
add_dependencies(test_coroutine ServerBin)
redefine_file_macro(test_coroutine)
target_link_libraries(test_coroutine ${LIBS})

add_executable(test_hook tests/test_hook.cc)
add_dependencies(test_hook ServerBin)
redefine_file_macro(test_hook)
target_link_libraries(test_hook ${LIBS})

add_executable(test_scheduler tests/test_scheduler.cc)
add_dependencies(test_scheduler ServerBin)
redefine_file_macro(test_scheduler)
target_link_libraries(test_scheduler ${LIBS})

add_executable(test_iomanager tests/test_iomanager.cc)
add_dependencies(test_iomanager ServerBin)
redefine_file_macro(test_iomanager)
target_link_libraries(test_iomanager ${LIBS})

add_executable(test_address tests/test_address.cc)
add_dependencies(test_address ServerBin)
redefine_file_macro(test_address)
target_link_libraries(test_address ${LIBS})

add_executable(test_socket tests/test_socket.cc)
add_dependencies(test_socket ServerBin)
redefine_file_macro(test_socket)
target_link_libraries(test_socket ${LIBS})

add_executable(test_bytearray tests/test_bytearray.cc)
add_dependencies(test_bytearray ServerBin)
redefine_file_macro(test_bytearray)
target_link_libraries(test_bytearray ${LIBS})

add_executable(test_http tests/test_http.cc)
add_dependencies(test_http ServerBin)
redefine_file_macro(test_http)
target_link_libraries(test_http ${LIBS})

add_executable(test_http_parser tests/test_http_parser.cc)
add_dependencies(test_http_parser ServerBin)
redefine_file_macro(test_http_parser)
target_link_libraries(test_http_parser ${LIBS})

add_executable(test_tcp_server tests/test_tcp_server.cc)
add_dependencies(test_tcp_server ServerBin)
redefine_file_macro(test_tcp_server)
target_link_libraries(test_tcp_server ${LIBS})

add_executable(echo_server examples/echo_server.cc)
add_dependencies(echo_server ServerBin)
redefine_file_macro(echo_server)
target_link_libraries(echo_server ${LIBS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

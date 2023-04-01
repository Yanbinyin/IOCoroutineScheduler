cmake_minimum_required(VERSION 2.8)
project(LibTim)

include (cmake/utils.cmake)

set(CMAKE_VERSION_FILE ON)  #make过程中显示详细命令
# set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function") # 为了方便调试输出信息，取消优化
# set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -g -O0 -Wall -Wno-builtin-macro-redefined")

include_directories(.)
include_directories(/apps/LibTim/include)   # yaml安装的文件夹，我好像安装在lib里面了，所以不需要
link_directories(/apps/LibTim/lib)

set(CMAKE_BUILD_TYPE Debug)

set(LIB_SRC 
    IOCoroutineScheduler/address.cc
    IOCoroutineScheduler/bytearray.cc
    IOCoroutineScheduler/config.cc
    IOCoroutineScheduler/coroutine.cc
    IOCoroutineScheduler/fd_manager.cc
    IOCoroutineScheduler/hook.cc
    IOCoroutineScheduler/http/http.cc
    IOCoroutineScheduler/http/http_parser.cc
    # IOCoroutineScheduler/http/http11_parser.rl.cc
    # IOCoroutineScheduler/http/httpclient_parser.rl.cc
    IOCoroutineScheduler/http/http_connection.cc
    IOCoroutineScheduler/http/http_server.cc
    IOCoroutineScheduler/http/http_session.cc
    IOCoroutineScheduler/http/servlet.cc
    # IOCoroutineScheduler/http/servlet/config_servlet.cc
    # IOCoroutineScheduler/http/servlet/status_servlet.cc
    IOCoroutineScheduler/iomanager.cc
    IOCoroutineScheduler/log.cc
    IOCoroutineScheduler/mutex.cc
    IOCoroutineScheduler/scheduler.cc
    IOCoroutineScheduler/socket.cc
    IOCoroutineScheduler/stream.cc
    # IOCoroutineScheduler/streams/zlib_stream.cc
    IOCoroutineScheduler/streams/socket_stream.cc
    IOCoroutineScheduler/thread.cc
    IOCoroutineScheduler/timer.cc
    IOCoroutineScheduler/tcp_server.cc
    # IOCoroutineScheduler/uri.rl.cpp
    IOCoroutineScheduler/util.cc
    IOCoroutineScheduler/util/crypto_util.cc
    IOCoroutineScheduler/util/json_util.cc
    IOCoroutineScheduler/util/hash_util.cc
)

ragelmaker(IOCoroutineScheduler/http/http11_parser.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/IOCoroutineScheduler/http)
ragelmaker(IOCoroutineScheduler/http/httpclient_parser.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/IOCoroutineScheduler/http)
ragelmaker(IOCoroutineScheduler/uri.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/IOCoroutineScheduler)

add_library(LibTim SHARED ${LIB_SRC})    #生成lib
redefine_file_macro(LibTim)  #__FILE__
# add_library(LibTim_static STATIC ${LIB_SRC})
# SET_TARGET_PROPERTIES(LibTim_static PROPERTIONS OUTPUT_NAME "IOCoroutineScheduler")


find_library(YAMLCPP yaml-cpp)
find_library(PTHREAD pthread)



set(LIBS
    LibTim
    dl #编译 dlsym（man dlsym查看）
    pthread
    yaml-cpp
    jsoncpp
    ssl
    protobuf
    )

message("***"  ${LIBS})

# function(LibTim_add_executable targetname srcs depends libs)
#     add_executable(${targetname} ${srcs})
#     add_dependencies(${targetname} ${depends})
#     force_redefine_file_macro_for_sources(${targetname})
#     target_link_libraries(${targetname} ${libs})
# endfunction()
###########  相当于下面的示例  #################
# add_executable(test tests/test.cc)      #生成测试文件
# add_dependencies(test LibTim)            #测试文件依赖于lib
# redefine_file_macro(test)
# target_link_libraries(test ${LIBS})

LibTim_add_executable(test_http_server "tests/test_http_server.cc" bin "${LIBS}")
LibTim_add_executable(test_http_connection "tests/test_http_connection.cc" bin "${LIBS}")
LibTim_add_executable(test_uri "tests/test_uri.cc" bin "${LIBS}")

add_executable(test tests/test.cc)      #生成测试文件
add_dependencies(test LibTim)            #测试文件依赖于lib
redefine_file_macro(test)
target_link_libraries(test ${LIBS})

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config LibTim)
redefine_file_macro(test_config)
target_link_libraries(test_config ${LIBS})

add_executable(test_thread tests/test_thread.cc)
add_dependencies(test_thread LibTim)
redefine_file_macro(test_thread)
target_link_libraries(test_thread ${LIBS})

add_executable(test_util tests/test_util.cc)
add_dependencies(test_util LibTim)
redefine_file_macro(test_util)
target_link_libraries(test_util ${LIBS})

add_executable(test_coroutine tests/test_coroutine.cc)
add_dependencies(test_coroutine LibTim)
redefine_file_macro(test_coroutine)
target_link_libraries(test_coroutine ${LIBS})

add_executable(test_hook tests/test_hook.cc)
add_dependencies(test_hook LibTim)
redefine_file_macro(test_hook)
target_link_libraries(test_hook ${LIBS})

add_executable(test_scheduler tests/test_scheduler.cc)
add_dependencies(test_scheduler LibTim)
redefine_file_macro(test_scheduler)
target_link_libraries(test_scheduler ${LIBS})

add_executable(test_iomanager tests/test_iomanager.cc)
add_dependencies(test_iomanager LibTim)
redefine_file_macro(test_iomanager)
target_link_libraries(test_iomanager ${LIBS})

add_executable(test_address tests/test_address.cc)
add_dependencies(test_address LibTim)
redefine_file_macro(test_address)
target_link_libraries(test_address ${LIBS})

add_executable(test_socket tests/test_socket.cc)
add_dependencies(test_socket LibTim)
redefine_file_macro(test_socket)
target_link_libraries(test_socket ${LIBS})

add_executable(test_bytearray tests/test_bytearray.cc)
add_dependencies(test_bytearray LibTim)
redefine_file_macro(test_bytearray)
target_link_libraries(test_bytearray ${LIBS})

add_executable(test_http tests/test_http.cc)
add_dependencies(test_http LibTim)
redefine_file_macro(test_http)
target_link_libraries(test_http ${LIBS})

add_executable(test_http_parser tests/test_http_parser.cc)
add_dependencies(test_http_parser LibTim)
redefine_file_macro(test_http_parser)
target_link_libraries(test_http_parser ${LIBS})

add_executable(test_tcp_server tests/test_tcp_server.cc)
add_dependencies(test_tcp_server LibTim)
redefine_file_macro(test_tcp_server)
target_link_libraries(test_tcp_server ${LIBS})

add_executable(echo_server examples/echo_server.cc)
add_dependencies(echo_server LibTim)
redefine_file_macro(echo_server)
target_link_libraries(echo_server ${LIBS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

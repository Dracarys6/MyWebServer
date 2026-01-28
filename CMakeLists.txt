cmake_minimum_required(VERSION 3.10)
project(WebServer)

# 使用C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 开启协程支持 (GCC 10/11 需要 -fcoroutines)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines -Wall -O2 -pthread")

# 添加头文件查找路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 手动指定要编译的源文件
set(SOURCES
${PROJECT_SOURCE_DIR}/src/main.cpp
${PROJECT_SOURCE_DIR}/src/Socket.cpp
${PROJECT_SOURCE_DIR}/src/Epoll.cpp
# ...
)

# 链接 MySQL库
# link_libraries(mysqlcilent)

#添加可执行文件 (第一个参数是可执行程序名，第二个是源文件列表)
add_executable(server ${SOURCES})

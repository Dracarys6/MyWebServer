cmake_minimum_required(VERSION 3.10)
project(WebServer)

# 使用C++20，强制开启
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加头文件查找路径（你的原有配置，保留）
include_directories(${PROJECT_SOURCE_DIR}/include)

# 手动指定要编译的源文件（你的原有配置，保留）
set(SOURCES
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${PROJECT_SOURCE_DIR}/src/Socket.cpp
    ${PROJECT_SOURCE_DIR}/src/Epoll.cpp
    ${PROJECT_SOURCE_DIR}/src/EventLoop.cpp
)

# 先定义可执行目标（必须在前）
add_executable(server ${SOURCES})

# 再对目标配置编译选项（优先级最高，覆盖全局FLAGS）
# 调试阶段核心：-O0 关闭优化 + -g 调试信息，必选！
target_compile_options(server PRIVATE
    -g            # 调试信息，gdb必备
    -O0           # 关闭优化，保证调试行号/变量准确，调试阶段绝不用O2/O3
    -fcoroutines  # GCC C++20协程支持
    -Wall         # 开启所有警告，提前发现空指针/未初始化变量等问题
)

# 第三步：配置目标链接选项（替代全局CMAKE_CXX_FLAGS中的-pthread，更规范）
target_link_libraries(server PRIVATE
    pthread
    # mysqlclient  
)
cmake_minimum_required(VERSION 3.10)
project(WebServer)

# ========== 基础 C++20 配置 ==========
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 强制纯 C++20 标准

# ========== macOS Clang 专属编译参数 ==========
# 移除 -fcoroutines-ts（新版 Clang 无需），仅保留必需参数
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
    -Wall -O2 -pthread \
    -arch arm64")

# ========== 头文件路径 ==========
include_directories(${PROJECT_SOURCE_DIR}/include)
# M1 MySQL 头文件（按需保留）
include_directories(/opt/homebrew/include/mysql)

# ========== 源文件列表 ==========
set(SOURCES
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    # 其他源文件添加在这里
)

# ========== 生成可执行文件 ==========
add_executable(server ${SOURCES})

# ========== 库链接 ==========
link_directories(/opt/homebrew/lib)
target_link_libraries(server mysqlclient)  # 按需保留
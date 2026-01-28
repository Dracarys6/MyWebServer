#!/bin/bash
# ==============================================================================
# C++ 项目通用编译脚本（Linux/macOS）
# 功能：自动创建build目录、执行cmake配置、多核编译、清理产物、编译后自动运行可执行文件
# 使用方式：
# 1. 赋予执行权限：chmod +x build.sh
# 2. 基本使用：./build.sh                （编译+自动运行）
# 3. 指定编译模式：./build.sh Debug      （Debug模式编译+自动运行）
# 4. 仅编译不运行：./build.sh no-run     （Release模式，仅编译）
# 5. 清理编译产物：./build.sh clean
# 6. 显示帮助：./build.sh help
# ==============================================================================

# 定义颜色（可选，用于输出提示）
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # 重置颜色

# 项目根目录（脚本所在目录）
PROJECT_ROOT=$(cd $(dirname $0); pwd)
# build目录路径
BUILD_DIR="${PROJECT_ROOT}/build"
# 默认编译模式
BUILD_TYPE="Release"
# 默认自动运行可执行文件
RUN_AFTER_BUILD=true
EXECUTABLE_NAME="server"  # 可执行程序
EXECUTABLE_PATH="${BUILD_DIR}/${EXECUTABLE_NAME}" # 可执行文件路径

# 帮助信息
show_help() {
    echo -e "${YELLOW}使用说明：${NC}"
    echo "  ./build.sh                - 以Release模式编译并自动运行程序"
    echo "  ./build.sh Debug          - 以Debug模式编译并自动运行程序"
    echo "  ./build.sh no-run         - 以Release模式编译，不运行程序"
    echo "  ./build.sh Debug no-run   - 以Debug模式编译，不运行程序"
    echo "  ./build.sh clean          - 清理编译产物（删除build目录）"
    echo "  ./build.sh help           - 显示帮助信息"
    echo -e "${YELLOW}注意：${NC}请确保EXECUTABLE_NAME变量匹配你的可执行程序名！"
}

# 清理编译产物
clean_build() {
    if [ -d "${BUILD_DIR}" ]; then
        echo -e "${YELLOW}正在清理编译产物：${BUILD_DIR}${NC}"
        rm -rf "${BUILD_DIR}"
        echo -e "${GREEN}清理完成！${NC}"
    else
        echo -e "${YELLOW}build目录不存在，无需清理${NC}"
    fi
}

# 运行可执行文件
run_executable() {
    if [ "${RUN_AFTER_BUILD}" = false ]; then
        echo -e "${YELLOW}跳过自动运行程序${NC}"
        return 0
    fi

    # 检查可执行文件是否存在
    if [ ! -f "${EXECUTABLE_PATH}" ]; then
        echo -e "${RED}错误：可执行文件不存在！路径：${EXECUTABLE_PATH}${NC}"
        echo -e "${YELLOW}请检查：${NC}"
        echo "  可执行程序是否输出到 ${BUILD_DIR} 目录"
        return 1
    fi

    # 赋予可执行权限（保险）
    chmod +x "${EXECUTABLE_PATH}"

    # 运行程序
    echo -e "${GREEN}==================== 程序开始运行 ====================${NC}"
    "${EXECUTABLE_PATH}"
    RUN_EXIT_CODE=$?
    echo -e "${GREEN}==================== 程序运行结束 ====================${NC}"
    echo -e "${YELLOW}程序退出码：${RUN_EXIT_CODE}${NC}"

    return ${RUN_EXIT_CODE}
}

# 主编译逻辑
build_project() {
    # 检查编译模式是否合法
    if [ "${BUILD_TYPE}" != "Debug" ] && [ "${BUILD_TYPE}" != "Release" ]; then
        echo -e "${RED}错误：无效的编译模式 '${BUILD_TYPE}'，仅支持 Debug/Release${NC}"
        show_help
        exit 1
    fi

    # 创建build目录（不存在则创建）
    if [ ! -d "${BUILD_DIR}" ]; then
        echo -e "${YELLOW}创建build目录：${BUILD_DIR}${NC}"
        mkdir -p "${BUILD_DIR}"
    fi

    # 进入build目录执行cmake
    echo -e "${YELLOW}进入build目录，执行cmake配置（${BUILD_TYPE}模式）${NC}"
    cd "${BUILD_DIR}" || {
        echo -e "${RED}错误：无法进入build目录 ${BUILD_DIR}${NC}"
        exit 1
    }

    # 执行cmake（指定编译模式）
    cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ..
    if [ $? -ne 0 ]; then
        echo -e "${RED}错误：cmake配置失败！${NC}"
        exit 1
    fi

    # 多核编译（自动检测CPU核心数）
    CORE_NUM=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    echo -e "${YELLOW}开始编译（使用 ${CORE_NUM} 个核心）${NC}"
    make -j${CORE_NUM}
    if [ $? -ne 0 ]; then
        echo -e "${RED}错误：编译失败！${NC}"
        exit 1
    fi

    echo -e "${GREEN}编译成功！可执行程序路径：${EXECUTABLE_PATH}${NC}"

    # 编译完成后运行程序
    run_executable
    if [ $? -ne 0 ]; then
        echo -e "${RED}程序运行出错！${NC}"
        exit 1
    fi

    exit 0
}

# 解析命令行参数
parse_args() {
    for arg in "$@"; do
        case "$arg" in
            clean)
                clean_build
                exit 0
                ;;
            Debug|Release)
                BUILD_TYPE="$arg"
                ;;
            no-run)
                RUN_AFTER_BUILD=false
                ;;
            help|--help|-h)
                show_help
                exit 0
                ;;
            *)
                echo -e "${RED}错误：无效参数 '${arg}'${NC}"
                show_help
                exit 1
                ;;
        esac
    done
}

# 主逻辑
parse_args "$@"
build_project
cmake_minimum_required(VERSION 3.16)
project(MoDriverPC_EtherCAT LANGUAGES C CXX)

# 设置 C/C++ 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成 compile_commands.json，方便代码补全
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加 SOEM 子模块
add_subdirectory(third_party/SOEM)

# 公共头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/SOEM/soem
)

# 源文件
set(SOURCES
    src/main.cpp
)

# 可执行文件
add_executable(ecat_scan ${SOURCES})

# 链接 SOEM 和线程库
target_link_libraries(ecat_scan
    soem
    pthread
)

# 安装目标（可选）
install(TARGETS ecat_scan
    RUNTIME DESTINATION bin
)
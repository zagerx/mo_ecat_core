cmake_minimum_required(VERSION 3.16)
project(ecat_cli VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 将顶层核心库作为子目录构建，产物放在当前 build 目录下的 core/ 中
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_BINARY_DIR}/core
)

add_executable(ecat_cli main.c)

target_link_libraries(ecat_cli PRIVATE mo_ecat_core)

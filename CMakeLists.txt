cmake_minimum_required(VERSION 3.16)
project(MoDriverPC_EtherCAT LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third_party/SOEM)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/SOEM/soem
)

# 公共源文件
set(ECAT_SOURCES
    src/ec_master/ec_master.cpp
    src/ec_controller/ec_controller.cpp
    src/cyclic/cyclic_runner.cpp
    src/utils/signal_handler.cpp
)

# ecat_scan: 主程序启动器
add_executable(ecat_scan src/main.cpp ${ECAT_SOURCES})
target_include_directories(ecat_scan PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ecat_scan
    soem
    pthread
)

# pdo_test: PDO 测试示例程序
add_executable(pdo_test src/examples/pdo_test.cpp ${ECAT_SOURCES})
target_include_directories(pdo_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(pdo_test
    soem
    pthread
)

# 编译警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ecat_scan PRIVATE
        -Wall
        -Wextra
    )
    target_compile_options(pdo_test PRIVATE
        -Wall
        -Wextra
    )
endif()

install(TARGETS ecat_scan pdo_test
    RUNTIME DESTINATION bin
)

cmake_minimum_required(VERSION 3.16)
project(ecat_cli VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT TARGET mo_ecat_core)
    # 支持在 examples/cli 目录下独立构建。
    set(MO_ECAT_BUILD_EXAMPLES OFF CACHE BOOL "Build mo_ecat examples" FORCE)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_BINARY_DIR}/core
    )
endif()

add_executable(ecat_cli
    main.c
    threads.c
    print.c
    commands.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config/mo_ecat_master_cfg.c
)

target_include_directories(ecat_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config
)

target_link_libraries(ecat_cli PRIVATE mo_ecat_core mo_robot pthread)

cmake_minimum_required(VERSION 3.16)
project(mo_ecat_core VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MO_ECAT_BUILD_EXAMPLES "Build mo_ecat examples" ON)

add_subdirectory(third_party/SOEM)

set(MO_ECAT_CORE_SOURCES
    src/mo_ecat_master.c
    src/backend/soem_backend.c
)

add_library(mo_ecat_core STATIC ${MO_ECAT_CORE_SOURCES})

target_include_directories(mo_ecat_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mo_ecat_core
    PUBLIC soem
    PUBLIC pthread
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mo_ecat_core PRIVATE
        -Wall
        -Wextra
    )
endif()

if(MO_ECAT_BUILD_EXAMPLES)
    add_subdirectory(examples/cli)
endif()

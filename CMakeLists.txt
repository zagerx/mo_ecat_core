cmake_minimum_required(VERSION 3.16)
project(mo_ecat_core VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MO_ECAT_BUILD_EXAMPLES "Build mo_ecat examples" ON)

add_subdirectory(third_party/SOEM)

set(MO_ECAT_CORE_SOURCES
    src/config/mo_ecat_master_cfg.c
    src/mo_ecat_master.c
    src/master_resources.c
    src/master_error.c
    src/master_topology.c
    src/master_pdo_mapping.c
    src/mo_ecat_cyclic.c
    src/mo_ecat_topology.c
    src/master_states.c
    src/common/statemachine/statemachine.c
    src/backend/backend.c
)

option(MO_ECAT_BACKEND_SOEM "Use SOEM backend" ON)

if(MO_ECAT_BACKEND_SOEM)
    list(APPEND MO_ECAT_CORE_SOURCES
        src/backend/soem/soem_backend.c
        src/backend/soem/soem_discovery.c
        src/backend/soem/soem_mapping.c
        src/backend/soem/soem_cyclic.c
    )
endif()

if(NOT MO_ECAT_BACKEND_SOEM)
    message(FATAL_ERROR "At least one backend must be selected")
endif()

add_library(mo_ecat_core STATIC ${MO_ECAT_CORE_SOURCES})

target_include_directories(mo_ecat_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mo_ecat_core
    PUBLIC soem
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mo_ecat_core PRIVATE
        -Wall
        -Wextra
    )
endif()

add_subdirectory(applications/robot)

if(MO_ECAT_BUILD_EXAMPLES)
    add_subdirectory(examples/cli)
endif()

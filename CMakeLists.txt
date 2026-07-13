cmake_minimum_required(VERSION 3.16)
project(mo_ecat_core VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MO_ECAT_BUILD_EXAMPLES "Build mo_ecat examples" ON)

add_subdirectory(third_party/SOEM)

# ---------------------------------------------------------------------------
# Configuration generator
# ---------------------------------------------------------------------------
# Products/embedded builds can point to a pre-built host generator by setting
# the environment variable MO_ECAT_CONFIG_GEN. Otherwise we build the generator
# as a normal host target.
if(DEFINED ENV{MO_ECAT_CONFIG_GEN} AND EXISTS "$ENV{MO_ECAT_CONFIG_GEN}")
    set(MO_ECAT_CONFIG_GEN_HOST_TOOL "$ENV{MO_ECAT_CONFIG_GEN}" CACHE FILEPATH "Host config generator" FORCE)
    set(MO_ECAT_CONFIG_GEN_DEPENDS "")
else()
    add_subdirectory(examples/tool/config_gen)
    set(MO_ECAT_CONFIG_GEN_HOST_TOOL "$<TARGET_FILE:mo_ecat_config_gen>")
    set(MO_ECAT_CONFIG_GEN_DEPENDS "mo_ecat_config_gen")
endif()

set(MO_ECAT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(MO_ECAT_GENERATED_MASTER_H "${MO_ECAT_GENERATED_DIR}/mo_ecat_master_cfg.h")
set(MO_ECAT_GENERATED_MASTER_C "${MO_ECAT_GENERATED_DIR}/mo_ecat_master_cfg.c")
set(MO_ECAT_GENERATED_ROBOT_H "${MO_ECAT_GENERATED_DIR}/robot_cfg.h")
set(MO_ECAT_GENERATED_ROBOT_C "${MO_ECAT_GENERATED_DIR}/robot_cfg.c")

add_custom_command(
    OUTPUT
        ${MO_ECAT_GENERATED_MASTER_H}
        ${MO_ECAT_GENERATED_MASTER_C}
        ${MO_ECAT_GENERATED_ROBOT_H}
        ${MO_ECAT_GENERATED_ROBOT_C}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MO_ECAT_GENERATED_DIR}
    COMMAND ${MO_ECAT_CONFIG_GEN_HOST_TOOL}
        --master-yaml ${CMAKE_CURRENT_SOURCE_DIR}/config/master.yaml
        --robot-yaml ${CMAKE_CURRENT_SOURCE_DIR}/applications/robot/config/robot.yaml
        --master-out-h ${MO_ECAT_GENERATED_MASTER_H}
        --master-out-c ${MO_ECAT_GENERATED_MASTER_C}
        --robot-out-h ${MO_ECAT_GENERATED_ROBOT_H}
        --robot-out-c ${MO_ECAT_GENERATED_ROBOT_C}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/config/master.yaml
        ${CMAKE_CURRENT_SOURCE_DIR}/applications/robot/config/robot.yaml
        ${MO_ECAT_CONFIG_GEN_DEPENDS}
    COMMENT "Generating mo_ecat configuration from YAML"
)

add_custom_target(mo_ecat_generated DEPENDS
    ${MO_ECAT_GENERATED_MASTER_H}
    ${MO_ECAT_GENERATED_MASTER_C}
    ${MO_ECAT_GENERATED_ROBOT_H}
    ${MO_ECAT_GENERATED_ROBOT_C}
)

set(MO_ECAT_CORE_SOURCES
    src/mo_ecat_master.c
    src/master_discovery.c
    src/mo_ecat_pdo.c
    src/mo_ecat_slave.c
    src/master_states.c
    src/common/statemachine/statemachine.c
)

option(MO_ECAT_BACKEND_SOEM "Use SOEM backend" ON)

if(MO_ECAT_BACKEND_SOEM)
    list(APPEND MO_ECAT_CORE_SOURCES src/backend/soem.c)
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
    PUBLIC pthread
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mo_ecat_core PRIVATE
        -Wall
        -Wextra
    )
endif()

# ---------------------------------------------------------------------------
# Generated configuration library
# ---------------------------------------------------------------------------
add_library(mo_ecat_config STATIC
    ${MO_ECAT_GENERATED_MASTER_C}
    ${MO_ECAT_GENERATED_ROBOT_C}
)

set_source_files_properties(
    ${MO_ECAT_GENERATED_MASTER_C}
    ${MO_ECAT_GENERATED_ROBOT_C}
    PROPERTIES GENERATED TRUE
)

target_include_directories(mo_ecat_config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/applications/robot>
        $<BUILD_INTERFACE:${MO_ECAT_GENERATED_DIR}>
)

add_dependencies(mo_ecat_config mo_ecat_generated)

add_subdirectory(applications/robot)

if(MO_ECAT_BUILD_EXAMPLES)
    add_subdirectory(examples/cli)
endif()

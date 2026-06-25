cmake_minimum_required(VERSION 3.16)
project(MoDriverPC_EtherCAT LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third_party/SOEM)

option(MO_ECAT_BUILD_CLI "Build transitional CLI example" ON)
option(MO_ECAT_BUILD_TESTS "Build tests" OFF)

set(MO_ECAT_CORE_SOURCES
    src/core/mo_ecat_master.cpp
    src/activity/activity_scheduler.cpp
    src/activity/pdo_mapping_activity.cpp
    src/activity/sdo_diagnostics_activity.cpp
    src/activity/sdo_parameter_activity.cpp
    src/activity/state_inspection_activity.cpp
    src/ec_master/ec_master.cpp
    src/ec_controller/ec_controller.cpp
    src/cyclic/cyclic_runner.cpp
    src/cyclic/process_data_engine.cpp
    src/servo_axis/servo_axis.cpp
    src/slave_node/device_profile.cpp
    src/slave_node/slave_config_factory.cpp
    src/slave_node/slave_node.cpp
    src/slave_node/slave_node_manager.cpp
    src/slave_node/slave_type_detector.cpp
    src/utils/logger.cpp
)

add_library(mo_ecat_core STATIC ${MO_ECAT_CORE_SOURCES})

target_include_directories(mo_ecat_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mo_ecat_core
    PUBLIC soem
    PRIVATE pthread
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mo_ecat_core PRIVATE
        -Wall
        -Wextra
    )
endif()

if(MO_ECAT_BUILD_CLI)
    add_executable(ecat_cli
        examples/cli/main.cpp
        examples/cli/ecat_application.cpp
        examples/cli/stdin_command_reader.cpp
    )

    target_link_libraries(ecat_cli PRIVATE mo_ecat_core)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ecat_cli PRIVATE
            -Wall
            -Wextra
        )
    endif()
endif()

if(MO_ECAT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

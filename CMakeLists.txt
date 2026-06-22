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

set(ECAT_SOURCES
    src/activity/sdo_diagnostics_activity.cpp
    src/activity/sdo_parameter_activity.cpp
    src/activity/state_inspection_activity.cpp
    src/app/ecat_application.cpp
    src/ec_master/ec_master.cpp
    src/ec_controller/ec_controller.cpp
    src/cyclic/cyclic_runner.cpp
    src/servo_axis/servo_axis.cpp
    src/slave_node/device_profile.cpp
    src/slave_node/slave_config_factory.cpp
    src/slave_node/slave_node.cpp
    src/slave_node/slave_node_manager.cpp
    src/slave_node/slave_type_detector.cpp
    src/utils/logger.cpp
)

add_executable(ecat_scan src/main.cpp ${ECAT_SOURCES})

target_include_directories(ecat_scan PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ecat_scan
    soem
    pthread
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ecat_scan PRIVATE
        -Wall
        -Wextra
    )
endif()

install(TARGETS ecat_scan
    RUNTIME DESTINATION bin
)

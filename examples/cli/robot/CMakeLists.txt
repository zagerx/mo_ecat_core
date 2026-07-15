add_library(ecat_cli_robot STATIC
	robot.c
)

target_include_directories(ecat_cli_robot
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(ecat_cli_robot PUBLIC mo_ecat_core)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(ecat_cli_robot PRIVATE -Wall -Wextra)
endif()

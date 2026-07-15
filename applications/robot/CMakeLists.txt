add_library(mo_robot STATIC
	robot.c
)

target_include_directories(mo_robot
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(mo_robot PUBLIC mo_ecat_core)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(mo_robot PRIVATE -Wall -Wextra)
endif()

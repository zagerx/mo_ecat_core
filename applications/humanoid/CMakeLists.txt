add_library(mo_humanoid STATIC
	robot_config.c
	robot.c
)

target_include_directories(mo_humanoid
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(mo_humanoid PUBLIC mo_ecat_core)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(mo_humanoid PRIVATE -Wall -Wextra)
endif()

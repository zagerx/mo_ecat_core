add_executable(mo_ecat_config_gen
    main.c
)

target_link_libraries(mo_ecat_config_gen PRIVATE yaml)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mo_ecat_config_gen PRIVATE -Wall -Wextra)
endif()

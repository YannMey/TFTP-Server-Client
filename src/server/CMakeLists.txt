add_executable(mytftp_server main.c ../utility/utils.c)


add_custom_command(TARGET mytftp_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/test/server/test_server
        ${CMAKE_CURRENT_BINARY_DIR}/test_server)
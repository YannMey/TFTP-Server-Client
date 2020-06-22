add_executable(mytftp_client main.c ../utility/utils.c)

add_custom_command(TARGET mytftp_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/test/client/test_client
        ${CMAKE_CURRENT_BINARY_DIR}/test_client)
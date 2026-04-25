add_executable(flasher WIN32
    src/hid.c
    src/protocol.c
    src/flasher.c
)

target_include_directories(flasher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_custom_command(TARGET flasher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:flasher> "${CMAKE_SOURCE_DIR}/bin/flasher.exe"
)

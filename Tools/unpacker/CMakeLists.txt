add_executable(unpacker src/unpacker.cpp)

target_include_directories(unpacker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_custom_command(TARGET unpacker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:unpacker> "${CMAKE_SOURCE_DIR}/bin/unpacker.exe"
)

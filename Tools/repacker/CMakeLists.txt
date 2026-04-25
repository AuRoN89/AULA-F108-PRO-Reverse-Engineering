add_executable(repacker src/repacker.cpp)

target_include_directories(repacker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_custom_command(TARGET repacker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:repacker> "${CMAKE_SOURCE_DIR}/bin/repacker.exe"
)

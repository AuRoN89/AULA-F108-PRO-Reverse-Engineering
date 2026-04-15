cmake_minimum_required(VERSION 3.10)
project(FirmwareTools VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Release)

# Add executable
add_executable(unpack unpack.cpp)
add_executable(repack repack.cpp)

# Includes
include_directories(${CMAKE_SOURCE_DIR}/include)

# Copy binaries to Tools/bin after build
add_custom_command(TARGET unpack POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:unpack> "${CMAKE_SOURCE_DIR}/../bin/unpack.exe"
)
add_custom_command(TARGET repack POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:repack> "${CMAKE_SOURCE_DIR}/../bin/repack.exe"
)

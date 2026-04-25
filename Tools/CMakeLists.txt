cmake_minimum_required(VERSION 3.10)
project(FirmwareTools VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Release)

add_subdirectory(unpacker)
add_subdirectory(repacker)
add_subdirectory(flasher)
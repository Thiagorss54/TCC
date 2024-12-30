cmake_minimum_required(VERSION 3.13)

add_subdirectory(libs/open62541)
include_directories(${CMAKE_BINARY_DIR}/libs/open62541)

add_subdirectory(udp/tests)
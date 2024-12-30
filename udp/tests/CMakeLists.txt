cmake_minimum_required(VERSION 3.13)
project("UDP Test")

include_directories(${CMAKE_BINARY_DIR}/libs/open62541)
include_directories(${CMAKE_BINARY_DIR}/libs/open62541/src_generated)


include_directories(${CMAKE_SOURCE_DIR}/libs/open62541/deps)
include_directories(${CMAKE_SOURCE_DIR}/libs/open62541/arch)
include_directories(${CMAKE_SOURCE_DIR}/libs/open62541/arch/posix)
include_directories(${CMAKE_SOURCE_DIR}/libs/open62541/include)
include_directories(${CMAKE_SOURCE_DIR}/libs/open62541/plugins)
include_directories(${CMAKE_SOURCE_DIR}/libs/open62541/src)
include_directories(${CMAKE_SOURCE_DIR}/libs/open62541/src/pubsub)
include_directories(${CMAKE_SOURCE_DIR}/libs/open62541/src/server)


add_executable(udp_pubsub_publish udp_pubsub_publish.c)
target_link_libraries(udp_pubsub_publish open62541)

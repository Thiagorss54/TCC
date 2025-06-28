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


add_executable(server_udp_pubsub server_udp_pubsub.c)
target_link_libraries(server_udp_pubsub open62541)

add_executable(client_udp_pubsub client_udp_pubsub.c)
target_link_libraries(client_udp_pubsub open62541)

add_executable(publish_mqtt tutorial_pubsub_mqtt_publish.c)
target_link_libraries(publish_mqtt open62541)

add_executable(subscribe_mqtt tutorial_pubsub_mqtt_subscribe.c)
target_link_libraries(subscribe_mqtt open62541)

add_executable(calc_lat_mqtt calc_lat_mqtt.c)
target_link_libraries(calc_lat_mqtt)

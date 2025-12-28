cmake_minimum_required(VERSION 3.10)

project(PubSubServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    server.cpp
    message_operations.cpp
    client_operations.cpp
    protocol_handler.cpp
)

set(HEADER_FILES
    common.h
    message_operations.h
    client_operations.h
    protocol_handler.h
)

add_executable(server_app ${SOURCE_FILES} ${HEADER_FILES})

find_package(Threads REQUIRED)
target_link_libraries(server_app PRIVATE Threads::Threads)

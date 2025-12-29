cmake_minimum_required(VERSION 3.10)

project(PubSubServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header search path
include_directories(include)

set(SOURCE_FILES
    src/server.cpp
    src/message_operations.cpp
    src/client_operations.cpp
    src/protocol_handler.cpp
)

set(HEADER_FILES
    include/common.h
    include/message_operations.h
    include/client_operations.h
    include/protocol_handler.h
)

add_executable(server_app ${SOURCE_FILES} ${HEADER_FILES})

find_package(Threads REQUIRED)
target_link_libraries(server_app PRIVATE Threads::Threads)

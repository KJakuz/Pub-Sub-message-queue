cmake_minimum_required(VERSION 3.12)
project(PubSubServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SERVER_SOURCES
    src/server.cpp
    src/message_operations.cpp
    src/client_operations.cpp
    src/protocol_handler.cpp
)

set(SERVER_HEADERS
    include/common.h
    include/message_operations.h
    include/client_operations.h
    include/protocol_handler.h
)

add_executable(server_app ${SERVER_SOURCES} ${SERVER_HEADERS})

target_include_directories(server_app
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(server_app PRIVATE -Wall -Wextra -Wpedantic)

find_package(Threads REQUIRED)
target_link_libraries(server_app PRIVATE Threads::Threads)

set_target_properties(server_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

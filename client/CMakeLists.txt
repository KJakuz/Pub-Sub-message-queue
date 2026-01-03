cmake_minimum_required(VERSION 3.10)
project(MessageQueueSystem VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

find_package(Threads REQUIRED)

set(LIB_SOURCES 
    src/message_queue_client.cpp 
    src/protocol.cpp
    src/helpers.cpp
)

add_library(mq_client SHARED ${LIB_SOURCES})

target_compile_options(mq_client PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(mq_client PRIVATE Threads::Threads)

add_executable(mq_demo main.cpp)

target_link_libraries(mq_demo mq_client)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
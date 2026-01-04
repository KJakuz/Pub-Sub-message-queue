cmake_minimum_required(VERSION 3.12)
project(PubSubMessageQueue)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default output dirs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Subdirectories
add_subdirectory(client)
add_subdirectory(server)

# Demo executable
add_executable(pubsub_demo main.cpp)

# Link the client library
target_link_libraries(pubsub_demo PRIVATE mq_client)

# Include directories for client headers
target_include_directories(pubsub_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/client/include)

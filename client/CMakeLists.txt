cmake_minimum_required(VERSION 3.12)
project(PubSubClient)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
set(CLIENT_SOURCES
    src/message_queue_client.cpp
    src/protocol.cpp
    src/helpers.cpp
)

set(CLIENT_HEADERS
    include/MessageQueueClient.h
    include/Protocol.h
    include/Event.h
    include/Helpers.h
)

# Static library
add_library(mq_client STATIC ${CLIENT_SOURCES} ${CLIENT_HEADERS})

# Include directories
target_include_directories(mq_client
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiler options
target_compile_options(mq_client PRIVATE -Wall -Wextra -Wpedantic)

# Threads
find_package(Threads REQUIRED)
target_link_libraries(mq_client PRIVATE Threads::Threads)

# Output directories
set_target_properties(mq_client PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

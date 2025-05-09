cmake_minimum_required(VERSION 3.10)
project(rdma_helpers)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add subdirectories
add_subdirectory(common)
add_subdirectory(rdma_objects)
add_subdirectory(rdma_connector)

# Create the main library
add_library(rdma_helpers SHARED
    qp_debug_lib.cpp
)

# Include directories
target_include_directories(rdma_helpers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(
    rdma_helpers
    rdma_common
    rdma_objects
    rdma_connector
)

# Debug level
option(ENABLE_DEBUG "Enable debug logs" ON)

if(ENABLE_DEBUG)
    add_definitions(-DLOG_LEVEL=LOG_LEVEL_DEBUG)
else()
    add_definitions(-DLOG_LEVEL=LOG_LEVEL_ERROR)
endif()
cmake_minimum_required(VERSION 3.10)
project(rdma_common)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library, so we use interface library
add_library(rdma_common INTERFACE)

# Include directories
target_include_directories(rdma_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
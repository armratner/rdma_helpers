cmake_minimum_required(VERSION 3.10)
project(rdma_general_objects)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(IBVERBS REQUIRED libibverbs)
pkg_check_modules(RDMACM REQUIRED librdmacm)
pkg_check_modules(MLX5 REQUIRED libmlx5)

# Source files
set(SOURCES
    rdma_device.cpp
    host.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${IBVERBS_INCLUDE_DIRS}
    ${RDMACM_INCLUDE_DIRS}
    ${MLX5_INCLUDE_DIRS}
)

# Library definition
add_library(rdma_general_objects STATIC ${SOURCES})

# Add library directory
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../rdma_objects/build)

# Link dependencies
target_link_libraries(rdma_general_objects
    rdma_objects
    ${IBVERBS_LIBRARIES}
    ${RDMACM_LIBRARIES}
    ${MLX5_LIBRARIES}
)

# Build the test executable
add_executable(general_objects_test test.cpp)
target_link_libraries(general_objects_test
    rdma_general_objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../rdma_objects/build/librdma_objects.a
    ${IBVERBS_LIBRARIES}
    ${RDMACM_LIBRARIES}
    ${MLX5_LIBRARIES}
)
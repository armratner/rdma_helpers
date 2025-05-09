cmake_minimum_required(VERSION 3.10)
project(rdma_connector)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(IBVERBS REQUIRED libibverbs)
pkg_check_modules(RDMACM REQUIRED librdmacm)
pkg_check_modules(MLX5 REQUIRED libmlx5)

# Path to rdma_objects library
set(RDMA_OBJECTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../rdma_objects/build)
link_directories(${RDMA_OBJECTS_DIR})

# Source files
set(SOURCES
    connector.cpp
    connection.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${IBVERBS_INCLUDE_DIRS}
    ${RDMACM_INCLUDE_DIRS}
    ${MLX5_INCLUDE_DIRS}
)

# Library definition
add_library(rdma_connector STATIC ${SOURCES})

# Link dependencies
target_link_libraries(rdma_connector
    rdma_objects
    ${IBVERBS_LIBRARIES}
    ${RDMACM_LIBRARIES}
    ${MLX5_LIBRARIES}
)

# Test executable
add_executable(rdma_connector_test test.cpp)
target_link_libraries(rdma_connector_test
    rdma_connector
    rdma_objects
    pthread
    ${IBVERBS_LIBRARIES}
    ${RDMACM_LIBRARIES}
    ${MLX5_LIBRARIES}
)

# Debug level
option(ENABLE_DEBUG "Enable debug logs" ON)

if(ENABLE_DEBUG)
    add_definitions(-DLOG_LEVEL=LOG_LEVEL_DEBUG)
else()
    add_definitions(-DLOG_LEVEL=LOG_LEVEL_ERROR)
endif()
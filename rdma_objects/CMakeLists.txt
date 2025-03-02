cmake_minimum_required(VERSION 3.10)
project(rdma_objects)

# Source files
set(LIB_SOURCES
    rdma_objects.cpp
)

set(HEADERS
    rdma_objects.h
    rdma_common.h
    auto_ref.h
)

# Test files
set(TEST_SOURCES
    test.cpp
)

# Create library
add_library(rdma_objects STATIC ${LIB_SOURCES})

# Create test executable
add_executable(rdma_objects_test ${TEST_SOURCES})

# Include directories
target_include_directories(rdma_objects 
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VERBS_INCLUDE_DIRS}
    ${RDMACM_INCLUDE_DIRS}
)

# Include directories for test
target_include_directories(rdma_objects_test
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VERBS_INCLUDE_DIRS}
    ${RDMACM_INCLUDE_DIRS}
)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(VERBS REQUIRED libibverbs)
pkg_check_modules(RDMACM REQUIRED librdmacm)

# Link libraries for the library
target_link_libraries(rdma_objects
    PUBLIC
    ${VERBS_LIBRARIES}
    ${RDMACM_LIBRARIES}
    mlx5
)

# Link libraries for the test
target_link_libraries(rdma_objects_test
    PRIVATE
    rdma_objects
    ${VERBS_LIBRARIES}
    ${RDMACM_LIBRARIES}
    mlx5
)

# Compile settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Install rules
install(TARGETS rdma_objects
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(TARGETS rdma_objects_test
    RUNTIME DESTINATION bin
)

# Compile definitions
target_compile_definitions(rdma_objects
    PUBLIC
    _GNU_SOURCE
    DEBUG
)

target_compile_definitions(rdma_objects_test
    PRIVATE
    _GNU_SOURCE
    DEBUG
)

# Compile options
target_compile_options(rdma_objects
    PRIVATE
    -Wall
    -Wextra
    -g
    -Wno-reorder
)

target_compile_options(rdma_objects_test
    PRIVATE
    -Wall
    -Wextra
    -g
    -Wno-reorder
)

# Debug level
option(ENABLE_DEBUG "Enable debug logs" ON)

if(ENABLE_DEBUG)
    add_definitions(-DLOG_LEVEL=LOG_LEVEL_DEBUG)
else()
    add_definitions(-DLOG_LEVEL=LOG_LEVEL_ERROR)
endif()
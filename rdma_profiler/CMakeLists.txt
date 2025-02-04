cmake_minimum_required(VERSION 3.10)
project(rdma_profiler)

# Add json dependency
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Source files
set(SOURCES
    main.cpp
    objects.cpp
    cm.cpp
)

set(HEADERS
    objects.h
    cm.h
    common.h
    auto_ref.h
    profiler.h
    profiler_singleton.h
)

# Create executable
add_executable(rdma_profiler ${SOURCES})

# Include directories
target_include_directories(rdma_profiler 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VERBS_INCLUDE_DIRS}
    ${RDMACM_INCLUDE_DIRS}
)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(VERBS REQUIRED libibverbs)
pkg_check_modules(RDMACM REQUIRED librdmacm)

# Link libraries
target_link_libraries(rdma_profiler
    PRIVATE
    ${VERBS_LIBRARIES}
    ${RDMACM_LIBRARIES}
    mlx5
    nlohmann_json::nlohmann_json
)

# Compile settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Install rules
install(TARGETS rdma_profiler
    RUNTIME DESTINATION bin
)

# Compile definitions
target_compile_definitions(rdma_profiler
    PRIVATE
    _GNU_SOURCE
    DEBUG
)

# Compile options
target_compile_options(rdma_profiler
    PRIVATE
    -Wall
    -Wextra
    -g
    -Wno-reorder
)
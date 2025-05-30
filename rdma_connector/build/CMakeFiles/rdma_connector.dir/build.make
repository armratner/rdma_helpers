# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.28

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/build

# Include any dependencies generated for this target.
include CMakeFiles/rdma_connector.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/rdma_connector.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/rdma_connector.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/rdma_connector.dir/flags.make

CMakeFiles/rdma_connector.dir/connector.cpp.o: CMakeFiles/rdma_connector.dir/flags.make
CMakeFiles/rdma_connector.dir/connector.cpp.o: /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/connector.cpp
CMakeFiles/rdma_connector.dir/connector.cpp.o: CMakeFiles/rdma_connector.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/rdma_connector.dir/connector.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/rdma_connector.dir/connector.cpp.o -MF CMakeFiles/rdma_connector.dir/connector.cpp.o.d -o CMakeFiles/rdma_connector.dir/connector.cpp.o -c /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/connector.cpp

CMakeFiles/rdma_connector.dir/connector.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/rdma_connector.dir/connector.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/connector.cpp > CMakeFiles/rdma_connector.dir/connector.cpp.i

CMakeFiles/rdma_connector.dir/connector.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/rdma_connector.dir/connector.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/connector.cpp -o CMakeFiles/rdma_connector.dir/connector.cpp.s

# Object files for target rdma_connector
rdma_connector_OBJECTS = \
"CMakeFiles/rdma_connector.dir/connector.cpp.o"

# External object files for target rdma_connector
rdma_connector_EXTERNAL_OBJECTS =

librdma_connector.a: CMakeFiles/rdma_connector.dir/connector.cpp.o
librdma_connector.a: CMakeFiles/rdma_connector.dir/build.make
librdma_connector.a: CMakeFiles/rdma_connector.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library librdma_connector.a"
	$(CMAKE_COMMAND) -P CMakeFiles/rdma_connector.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/rdma_connector.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/rdma_connector.dir/build: librdma_connector.a
.PHONY : CMakeFiles/rdma_connector.dir/build

CMakeFiles/rdma_connector.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/rdma_connector.dir/cmake_clean.cmake
.PHONY : CMakeFiles/rdma_connector.dir/clean

CMakeFiles/rdma_connector.dir/depend:
	cd /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/build /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/build /mtxswgwork/armeng/workspace/armen_dev/EasyRdma/rdma_helpers/rdma_connector/build/CMakeFiles/rdma_connector.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/rdma_connector.dir/depend


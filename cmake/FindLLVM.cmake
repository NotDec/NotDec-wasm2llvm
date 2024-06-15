
# https://github.com/banach-space/llvm-tutor/blob/main/CMakeLists.txt
#===============================================================================
# 1. VERIFY LLVM INSTALLATION DIR
# This is just a bit of a sanity check.
#===============================================================================

if(NOT NOTDEC_LLVM_INSTALL_DIR)
    set(NOTDEC_LLVM_INSTALL_DIR "/usr/lib/llvm-14/" CACHE PATH "LLVM installation directory")
endif()

# 1.1 Check the "include| directory
set(NOTDEC_LLVM_INCLUDE_DIR "${NOTDEC_LLVM_INSTALL_DIR}/include/llvm")
if(NOT EXISTS "${NOTDEC_LLVM_INCLUDE_DIR}")
message(FATAL_ERROR
	" NOTDEC_LLVM_INSTALL_DIR (${NOTDEC_LLVM_INCLUDE_DIR}) is invalid.")
endif()

# 1.2 Check that the LLVMConfig.cmake file exists (the location depends on the
# OS)
set(NOTDEC_LLVM_VALID_INSTALLATION FALSE)

# Ubuntu + Darwin
if(EXISTS "${NOTDEC_LLVM_INSTALL_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
	set(NOTDEC_LLVM_VALID_INSTALLATION TRUE)
endif()

# Fedora
if(EXISTS "${NOTDEC_LLVM_INSTALL_DIR}/lib64/cmake/llvm/LLVMConfig.cmake")
	set(NOTDEC_LLVM_VALID_INSTALLATION TRUE)
endif()

if(NOT ${NOTDEC_LLVM_VALID_INSTALLATION})
	message(FATAL_ERROR
		"LLVM installation directory, (${NOTDEC_LLVM_INSTALL_DIR}), is invalid. Couldn't
		find LLVMConfig.cmake.")
endif()

#===============================================================================
# 2. LOAD LLVM CONFIGURATION
#    For more: http://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
#===============================================================================
# Add the location of LLVMConfig.cmake to CMake search paths (so that
# find_package can locate it)
# Note: On Fedora, when using the pre-compiled binaries installed with `dnf`,
# LLVMConfig.cmake is located in "/usr/lib64/cmake/llvm". But this path is
# among other paths that will be checked by default when using
# `find_package(llvm)`. So there's no need to add it here.
list(APPEND CMAKE_PREFIX_PATH "${NOTDEC_LLVM_INSTALL_DIR}/lib/cmake/llvm/")

find_package(LLVM 14 REQUIRED CONFIG)

# See: https://stackoverflow.com/questions/55921707/setting-path-to-clang-library-in-cmake
list(APPEND CMAKE_PREFIX_PATH "${NOTDEC_LLVM_INSTALL_DIR}/lib/cmake/clang/")
find_package(Clang REQUIRED CONFIG)

# Another sanity check
if(NOT "14" VERSION_EQUAL "${LLVM_VERSION_MAJOR}")
	message(FATAL_ERROR "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 14")
endif()

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${NOTDEC_LLVM_INSTALL_DIR}")

message("LLVM STATUS:
	Definitions ${LLVM_DEFINITIONS}
	Includes    ${LLVM_INCLUDE_DIRS}
	Libraries   ${LLVM_LIBRARY_DIRS}
	Targets     ${LLVM_TARGETS_TO_BUILD}"
)

# Set the LLVM header and library paths
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Clang header and library paths
include_directories(SYSTEM ${CLANG_INCLUDE_DIRS})
link_directories(${CLANG_LIBRARY_DIRS})
add_definitions(${CLANG_DEFINITIONS})

#===============================================================================
# 3. LLVM-TUTOR BUILD CONFIGURATION
#===============================================================================
# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 17 CACHE STRING "")

# Build type
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE
			STRING "Build type (default Debug):" FORCE)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall\
	-fdiagnostics-color=always")

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# -fvisibility-inlines-hidden is set when building LLVM and on Darwin warnings
# are triggered if llvm-tutor is built without this flag (though otherwise it
# builds fine). For consistency, add it here too.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fvisibility-inlines-hidden" SUPPORTS_FVISIBILITY_INLINES_HIDDEN_FLAG)
if (${SUPPORTS_FVISIBILITY_INLINES_HIDDEN_FLAG} EQUAL "1")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
endif()

# Set the build directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

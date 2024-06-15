cmake_minimum_required(VERSION 3.13.4)
project(notdec-wasm2llvm
	LANGUAGES C CXX
	VERSION 0.0.1
)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(FindLLVM)

include_directories(${PROJECT_SOURCE_DIR}/include/notdec-wasm2llvm)
include_directories(${PROJECT_SOURCE_DIR}/include)

option(NOTDEC_LLVM2C_ENABLE_TESTS "Enable tests." ON)

# Add wabt

include(ExternalProject)
ExternalProject_add(
  wabt 
  URL https://github.com/WebAssembly/wabt/releases/download/1.0.33/wabt-1.0.33.tar.xz
  SOURCE_DIR ${CMAKE_BINARY_DIR}/wabt
  BINARY_DIR ${CMAKE_BINARY_DIR}/wabt-build
  INSTALL_COMMAND ""
  CMAKE_CACHE_ARGS "-DBUILD_LIBWASM:BOOL=ON "
  # https://stackoverflow.com/questions/50400592/using-an-externalproject-download-step-with-ninja
  BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/wabt-build/libwabt.a"
)
include_directories(${CMAKE_BINARY_DIR}/wabt/include)
include_directories(${CMAKE_BINARY_DIR}/wabt-build/include)
add_library(wabt::wabt SHARED IMPORTED)
set_property(TARGET wabt::wabt PROPERTY IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/wabt-build/libwabt.a")
# make the wabt library depend on the project, or the wabt will be built alongside our project, instead of before it.
add_dependencies(wabt::wabt wabt)

add_subdirectory(lib)
add_subdirectory(tools)

if (NOTDEC_LLVM2C_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# The wasm corpus needs the wasi-sdk clang to build the .wasm inputs; without it
# the suite cannot run, so report it instead of registering a test that is
# guaranteed to fail (a red ctest must mean a product problem).
if (NOT DEFINED WASI_CLANG)
  set(WASI_CLANG "/opt/wasi-sdk-20.0/bin/clang"
    CACHE FILEPATH "wasi-sdk clang used to build the SysY wasm corpus")
endif()
if (NOT EXISTS "${WASI_CLANG}")
  message(STATUS "skipping sysy-tests: wasi-sdk clang not found at ${WASI_CLANG}")
  return()
endif()

set(sysy_make_args clean test "WASI_CLANG=${WASI_CLANG}")
if (NOTDEC_TEST_CLANG)
  list(APPEND sysy_make_args "CLANG=${NOTDEC_TEST_CLANG}")
endif()
add_test (NAME sysy-tests
  COMMAND make ${sysy_make_args}
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

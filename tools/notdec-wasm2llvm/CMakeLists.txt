add_executable(notdec-wasm2llvm-exe
  notdec-wasm2llvm.cpp
)

target_link_libraries(notdec-wasm2llvm-exe
  notdec-wasm2llvm
)

if (NOTDEC_HAVE_SHARED_LLVM)
  target_link_libraries(notdec-wasm2llvm-exe
    notdec_llvm_deps
  )
else ()
  target_link_libraries(notdec-wasm2llvm-exe
    LLVMBitWriter
  )
endif ()

set_target_properties(notdec-wasm2llvm-exe
	PROPERTIES
		OUTPUT_NAME "notdec-wasm2llvm"
)

install(TARGETS notdec-wasm2llvm-exe DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_executable(notdec-wasm2llvm-exe
  notdec-wasm2llvm.cpp
)

target_link_libraries(notdec-wasm2llvm-exe 
  notdec-wasm2llvm LLVMBitWriter
)

set_target_properties(notdec-wasm2llvm-exe
	PROPERTIES
		OUTPUT_NAME "notdec-wasm2llvm"
)

install(TARGETS notdec-wasm2llvm-exe DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

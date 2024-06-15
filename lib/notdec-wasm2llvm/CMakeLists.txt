include(AddLLVM)
add_library(notdec-wasm2llvm SHARED STATIC
    parser-block.cpp
    parser-instruction.cpp
    parser.cpp
    utils.cpp
)

target_link_libraries(notdec-wasm2llvm
    PUBLIC
    wabt::wabt LLVMCore
)

set_target_properties(notdec-wasm2llvm
    PROPERTIES
        OUTPUT_NAME "notdec-wasm2llvm"
)

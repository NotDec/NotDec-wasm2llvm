include(AddLLVM)
add_library(notdec-wasm2llvm SHARED STATIC
    interface.cpp
    parser-block.cpp
    parser-instruction.cpp
    parser.cpp
    utils.cpp
)

target_link_libraries(notdec-wasm2llvm
    PUBLIC
    wabt::wabt
)

if (NOTDEC_HAVE_SHARED_LLVM)
    target_link_libraries(notdec-wasm2llvm
        PUBLIC
        notdec_llvm_deps
    )
else ()
    target_link_libraries(notdec-wasm2llvm
        PUBLIC
        LLVMCore
    )
endif ()

target_include_directories(notdec-wasm2llvm
    PUBLIC
    ../../include
)

set_target_properties(notdec-wasm2llvm
    PROPERTIES
        OUTPUT_NAME "notdec-wasm2llvm"
)

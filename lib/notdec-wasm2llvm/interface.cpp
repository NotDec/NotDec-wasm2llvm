
#include "interface.h"
#include "parser.h"

namespace notdec::frontend {

void parse_wasm(llvm::LLVMContext &llvmContext, llvm::Module &llvmModule,
                Options opts, std::string file_name) {
  notdec::frontend::wasm::parse_wasm(llvmContext, llvmModule, opts, file_name);
}

void parse_wat(llvm::LLVMContext &llvmContext, llvm::Module &llvmModule,
               Options opts, std::string file_name) {
  notdec::frontend::wasm::parse_wat(llvmContext, llvmModule, opts, file_name);
}

void free_buffer() {
  notdec::frontend::wasm::free_buffer();
}

} // namespace notdec::frontend

#ifndef _NOTDEC_FRONTEND_WASM_CONTEXT_H_
#define _NOTDEC_FRONTEND_WASM_CONTEXT_H_

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace notdec::frontend::wasm {

extern const char *LOCAL_PREFIX;
extern const char *PARAM_PREFIX;
extern const char *DEFAULT_FUNCNAME_PREFIX;

struct Options {
  /// (Breaks execution!) If true, generate inttoptr for memory access, instead
  /// of get element ptr of the global memory.
  bool GenIntToPtr : 1;
  /// If true, apply some name transformations, e.g. transform the
  /// __original_main/__main_argc_argv to main.
  bool FixNames : 1;
  /// If true, not remove the dollar prefix for names.
  bool NoRemoveDollar : 1;
  /// If true, rename function to its export name even if it has a name.
  /// By default, we use the shorter name between the name section name and the
  /// export name.
  bool ForceExportName : 1;
  /// (Breaks execution!) Split initialized parts of the memory to global
  /// variables.
  bool SplitMem : 1;
  /// (Breaks execution!) Do not generate memory initializer, because currently
  /// the initializer is flattened, which makes the bytecode really big in size.
  bool NoMemInitializer : 1;
  int LogLevel;
};

} // namespace notdec::frontend::wasm

namespace notdec::frontend {

using notdec::frontend::wasm::Options;

void parse_wasm(llvm::LLVMContext &llvmContext, llvm::Module &llvmModule,
                Options opts, std::string file_name);
void parse_wat(llvm::LLVMContext &llvmContext, llvm::Module &llvmModule,
               Options opts, std::string file_name);

} // namespace notdec::frontend

#endif

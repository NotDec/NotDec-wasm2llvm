#ifndef _NOTDEC_FRONTEND_WASM_PARSER_H_
#define _NOTDEC_FRONTEND_WASM_PARSER_H_

#include <cstdint>
#include <iostream>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <vector>

// wabt header
#include "wabt/binary-reader-ir.h"
#include "wabt/binary-reader.h"
#include "wabt/cast.h"
#include "wabt/ir.h"
#include "wabt/stream.h"
#include "wabt/validator.h"
#include "wabt/wast-parser.h"

#include "interface.h"
#include "utils.h"

namespace notdec::frontend::wasm {

struct Context {
  Options opts;
  llvm::LLVMContext &llvmContext;
  llvm::Module &llvmModule;
  std::unique_ptr<wabt::Module> module;
  // mapping from global index to llvm thing
  std::vector<llvm::GlobalVariable *> globs;
  std::vector<llvm::Function *> funcs;
  std::vector<llvm::GlobalVariable *> mems;
  std::vector<llvm::GlobalVariable *> tables;

  // Memory buffer for `ConstantDataArray::getRaw`.
  // TODO: Should be kept with llvmContext.
  std::vector<uint8_t *> OwnedBuffers;

  Context(llvm::LLVMContext &llvmContext, llvm::Module &llvmModule,
          Options opts)
      : opts(opts), llvmContext(llvmContext), llvmModule(llvmModule) {}

  ~Context() {
    for (auto *buf : OwnedBuffers) {
      delete[] buf;
    }
  }

  void visitModule();
  void visitGlobal(wabt::Global &gl, bool isExternal);
  void visitFunc(wabt::Func &func, llvm::Function *function);
  void visitTable(wabt::Table &table, bool isExternal);
  void visitElem(wabt::ElemSegment &elem);
  llvm::PointerType *getFuncPointerType() {
    return llvm::PointerType::get(
        llvm::FunctionType::get(llvm::Type::getVoidTy(llvmContext), false), 0);
  }
  llvm::Constant *visitInitExpr(wabt::ExprList &expr);
  llvm::GlobalVariable *visitDataSegment(wabt::DataSegment &ds);

  llvm::Function *declareFunc(wabt::Func &func, bool isExternal);
  llvm::GlobalVariable *declareMemory(wabt::Memory &mem, bool isExternal);
  void setFuncArgName(llvm::Function &function,
                      const wabt::FuncSignature &decl);
  llvm::Function *findFunc(wabt::Var &var);

private:
  wabt::Index _func_index = 0;
  wabt::Index _glob_index = 0;
  wabt::Index _mem_index = 0;
  wabt::Index _table_index = 0;
};

void free_buffer();
std::unique_ptr<Context> parse_wasm(llvm::LLVMContext &llvmContext,
                                    llvm::Module &llvmModule, Options opts,
                                    std::string file_name);
std::unique_ptr<Context> parse_wat(llvm::LLVMContext &llvmContext,
                                   llvm::Module &llvmModule, Options opts,
                                   std::string file_name);

llvm::Constant *convertZeroValue(llvm::LLVMContext &llvmContext,
                                 const wabt::Type &ty);
llvm::Type *convertType(llvm::LLVMContext &llvmContext, const wabt::Type &ty);
llvm::FunctionType *convertFuncType(llvm::LLVMContext &llvmContext,
                                    const wabt::FuncSignature &decl);
llvm::Type *convertReturnType(llvm::LLVMContext &llvmContext,
                              const wabt::FuncSignature &decl);
llvm::Constant *createMemInitializer(llvm::LLVMContext &llvmContext,
                                     llvm::Type *memty, uint64_t offset,
                                     std::vector<uint8_t> data);
void modMemInitializer(llvm::StringRef ptr, uint64_t offset,
                       std::vector<uint8_t> data);

std::string removeDollar(std::string name);
llvm::StringRef removeDollar(llvm::StringRef name);

} // namespace notdec::frontend::wasm
#endif

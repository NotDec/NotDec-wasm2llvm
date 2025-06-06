#include <cstdint>
#include <cstdlib>

#include <cstring>
#include <new>
#include <set>
#include <vector>
#include <string>

#include "wabt/base-types.h"
#include "wabt/ir.h"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>


#include "parser-block.h"
#include "parser.h"
#include "utils.h"

namespace notdec::frontend::wasm {

const char *DEFAULT_FUNCNAME_PREFIX = "func_";
const char *DEFAULT_TABLE_PREFIX = "table_";

std::vector<uint8_t *> GlobBuffers;

void free_buffer() {
  for (uint8_t *buffer : GlobBuffers) {
    delete[] buffer;
  }
  GlobBuffers.clear();
}

std::unique_ptr<Context> parse_wat(llvm::LLVMContext &llvmContext,
                                   llvm::Module &llvmModule, Options opts,
                                   std::string file_name) {
  using namespace wabt;
  std::vector<uint8_t> file_data;
  Result result = ReadFile(file_name, &file_data);
  std::unique_ptr<WastLexer> lexer = WastLexer::CreateBufferLexer(
      file_name, file_data.data(), file_data.size(), nullptr);

  if (!Succeeded(result)) {
    std::cerr << "Read wat file failed." << std::endl;
    return std::unique_ptr<Context>(nullptr);
  }
  // Context ctx(llvmCtx);
  std::unique_ptr<Context> ret =
      std::make_unique<Context>(llvmContext, llvmModule, opts);

  Errors errors;
  Features s_features;
  // std::unique_ptr<FileStream> s_log_stream = FileStream::CreateStderr();
  WastParseOptions options(s_features);
  result = ParseWatModule(lexer.get(), (&(ret->module)), &errors, &options);
  if (!Succeeded(result)) {
    std::cerr << "Read wat file failed." << std::endl;
    return std::unique_ptr<Context>(nullptr);
  }
  bool s_validate = true;
  if (s_validate) {
    ValidateOptions options(s_features);
    result = ValidateModule(ret->module.get(), &errors, options);
    if (!Succeeded(result)) {
      std::cerr << "Wat validation failed." << std::endl;
      return std::unique_ptr<Context>(nullptr);
    }
  }
  // do generation
  ret->visitModule();
  return ret;
}

std::unique_ptr<Context> parse_wasm(llvm::LLVMContext &llvmContext,
                                    llvm::Module &llvmModule, Options opts,
                                    std::string file_name) {
  using namespace wabt;
  // 这部分代码来自WABT，解析wasm文件
  std::vector<uint8_t> file_data;
  Result result = ReadFile(file_name, &file_data);

  std::unique_ptr<Context> ret =
      std::make_unique<Context>(llvmContext, llvmModule, opts);
  ret->module = std::make_unique<Module>();
  if (!Succeeded(result)) {
    std::cerr << "Read wasm file failed." << std::endl;
    return std::unique_ptr<Context>(nullptr);
  }
  Errors errors;
  const bool kStopOnFirstError = true;
  Features s_features;
  // std::unique_ptr<FileStream> s_log_stream = FileStream::CreateStderr();
  ReadBinaryOptions options(s_features, nullptr, // s_log_stream.get(),
                            true, kStopOnFirstError, true);
  result = ReadBinaryIr(file_name.c_str(), file_data.data(), file_data.size(),
                        options, &errors, ret->module.get());
  if (!Succeeded(result)) {
    std::cerr << "Read wasm file failed." << std::endl;
    return std::unique_ptr<Context>(nullptr);
  }
  bool s_validate = true;
  if (s_validate) {
    ValidateOptions options(s_features);
    result = ValidateModule(ret->module.get(), &errors, options);
    if (!Succeeded(result)) {
      std::cerr << "Wasm validation failed." << std::endl;
      return std::unique_ptr<Context>(nullptr);
    }
  }
  ret->visitModule();
  return ret;
}

template <typename T> void inline setNameIfEmpty(T &field, std::string &name) {
  if (field.name.empty()) {
    field.name = name;
  }
}

void Context::visitModule() {
  using namespace wabt;
  // TODO temporatily deduplicate.
  // global can be in both Global list and import section.
  std::set<Global *> visitedGlobals;

  // TODO detect wabt memory64 extension
  // funcrefs and externrefs have address space 10, 20 in LLVM Webassembly IR
  // datalayout target datalayout = "e-m:e-p:32:32-i64:64-n32:64-S128" e little
  // endian m:e mangling elf p:32:32 32bit pointer. S128 Stack natural alignment
  // 128bit

  // target triple = "wasm32-unknown-wasi"
  llvmModule.setDataLayout("e-m:e-p:32:32-i64:64-n32:64-S128");
  llvmModule.setTargetTriple("wasm32-unknown-wasi");

  // change module name from file name to wasm module name if there is
  if (!module->name.empty()) {
    llvmModule.setModuleIdentifier(module->name);
  }
  if (this->_func_index != 0 || this->_glob_index != 0) {
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: Cannot add module when globals is not empty"
              << std::endl;
    std::abort();
  }

  // visit imports & build function index map
  for (Import *import : this->module->imports) {
    std::string import_name = import->module_name + "." + import->field_name;
    switch (import->kind()) {
    case ExternalKind::Func: {
      auto &Field = cast<FuncImport>(import)->func;
      setNameIfEmpty(Field, import_name);
      declareFunc(Field, true);
    } break;

    case ExternalKind::Memory: {
      // create external memory variable
      auto &Field = cast<MemoryImport>(import)->memory;
      setNameIfEmpty(Field, import_name);
      declareMemory(Field, true);
    } break;

    case ExternalKind::Global: {
      // set global to external linkage
      auto &Field = cast<GlobalImport>(import)->global;
      setNameIfEmpty(Field, import_name);
      visitGlobal(Field, true);
      visitedGlobals.insert(&Field);
    } break;

    case ExternalKind::Table: {
      auto &Field = cast<TableImport>(import)->table;
      setNameIfEmpty(Field, import_name);
      visitTable(Field, true);
    } break;

    case ExternalKind::Tag:
    default:
      std::cerr << __FILE__ << ":" << __LINE__ << ": "
                << "Error: Unknown import kind: "
                << g_kind_name[static_cast<size_t>(import->kind())]
                << std::endl;
      // std::abort();
      break;
    }
  }
  // visit global
  for (Global *gl : this->module->globals) {
    if (visitedGlobals.count(gl) > 0) {
      continue;
    }
    visitGlobal(*gl, false);
  }

  // visit memory and data, create memory content
  // for (Memory* mem: this->module->memories) {
  //     declareMemory(*mem, false);
  // }
  for (ModuleField &field : module->fields) {
    // wabt\src\wat-writer.cc WatWriter::WriteModule
    if (field.type() != ModuleFieldType::Memory) {
      continue;
    }
    Memory &mem = cast<MemoryModuleField>(&field)->memory;
    declareMemory(mem, false);
  }

  for (ModuleField &field : module->fields) {
    if (field.type() != ModuleFieldType::DataSegment) {
      continue;
    }
    DataSegment &ds = cast<DataSegmentModuleField>(&field)->data_segment;
    visitDataSegment(ds);
  }

  // TODO data

  std::vector<llvm::Function *> nonImportFuncs;
  // iterate without import function
  // see wabt src\wat-writer.cc WatWriter::WriteModule
  for (ModuleField &field : module->fields) {
    if (field.type() != ModuleFieldType::Func) {
      continue;
    }
    Func &func = cast<FuncModuleField>(&field)->func;
    llvm::Function *function = declareFunc(func, false);
    if (opts.FixNames) {
      if (!opts.NoRemoveDollar && function->getName().front() == '$') {
        function->setName(removeDollar(function->getName()));
      }
      if (function->getName().equals("__original_main") ||
          function->getName().equals("__main_argc_argv")) {
        function->setLinkage(llvm::GlobalValue::LinkageTypes::ExternalLinkage);
        if (llvm::Function *func = this->llvmModule.getFunction("main")) {
          func->setName("");
        }
        function->setName("main");
      }
      // Temporary fix for the sysY test cases
      if (function->getName().equals("memset")) {
        function->setName("memset_1");
      }
      if (function->getName().equals("memcpy")) {
        function->setName("memcpy_1");
      }
    } else {
      // set main as external
      if (function->getName().equals("main")) {
        function->setLinkage(llvm::GlobalValue::LinkageTypes::ExternalLinkage);
      }
    }
    nonImportFuncs.push_back(function);
  }

  // visit elem and create function pointer array
  for (Table *field : this->module->tables) {
    visitTable(*field, false);
  }

  // visit function
  std::size_t i = 0;
  for (ModuleField &field : module->fields) {
    if (field.type() != ModuleFieldType::Func) {
      continue;
    }
    Func &func = cast<FuncModuleField>(&field)->func;
    visitFunc(func, nonImportFuncs.at(i));
    i++;
  }
  // for (Func* func: this->module.funcs) {
  //     std::cout << func->name << std::endl;
  //     std::cout << "  " << func->exprs.size() << std::endl;
  // }
  // visit export and change visibility
  for (Export *export_ : this->module->exports) {
    Index index;
    // Func* func;
    switch ((*export_).kind) {
    case ExternalKind::Func: {
      index = module->GetFuncIndex(export_->var);
      this->funcs[index]->setLinkage(
          llvm::GlobalValue::LinkageTypes::ExternalLinkage);
      // 之前internal的时候同时自动设置了dso_local
      this->funcs[index]->setDSOLocal(false);
      // rename func with its export name
      std::string &export_name = export_->name;
      if (!export_name.empty()) {
        if (opts.ForceExportName) {
          if (llvm::Function *func =
                  this->llvmModule.getFunction(export_name)) {
            func->setName("");
          }
          this->funcs[index]->setName(export_name);
        } else {
          // perfer shorter name
          // if (this->funcs[index]->getName().str().length() >
          //     export_name.length()) {
          // use export name if name is empty
          if (this->funcs[index]->getName().str().empty()) {
            this->funcs[index]->setName(export_name);
          }
        }
      }
      break;
    }
    case ExternalKind::Table:
      // TODO
      index = module->GetTableIndex(export_->var);
      break;

    case ExternalKind::Memory:
      index = module->GetMemoryIndex(export_->var);
      // this->mems[index]->setInitializer(nullptr);
      this->mems[index]->setLinkage(
          llvm::GlobalValue::LinkageTypes::ExternalLinkage);
      this->mems[index]->setDSOLocal(false);
      break;

    case ExternalKind::Global:
      index = module->GetGlobalIndex(export_->var);
      // this->globs[index]->setInitializer(nullptr);
      this->globs[index]->setLinkage(
          llvm::GlobalValue::LinkageTypes::ExternalLinkage);
      this->globs[index]->setDSOLocal(false);
      break;

    case ExternalKind::Tag:
      index = module->GetTagIndex(export_->var);
      break;

    default:
      break;
    }
  }

  // assign default names to functions
  for (std::size_t i = 0; i < this->funcs.size(); i++) {
    if (this->funcs[i]->getName().empty()) {
      this->funcs[i]->setName(DEFAULT_FUNCNAME_PREFIX + std::to_string(i));
    }
  }

  // elem段需要函数指针，所以依赖func段
  for (ElemSegment *elem : this->module->elem_segments) {
    visitElem(*elem);
  }
  assert((this->funcs.size() == _func_index));
}

llvm::GlobalVariable *Context::visitDataSegment(wabt::DataSegment &ds) {
  using namespace llvm;
  wabt::Index index = module->GetMemoryIndex(ds.memory_var);
  if (index >= _mem_index) {
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: data section mem ref out of bound." << index
              << std::endl;
    std::abort();
  }
  // LLVM side mem manipulation
  GlobalVariable *mem = this->mems.at(index);
  Type *memty = mem->getValueType();
  Constant *offset = visitInitExpr(ds.offset);

  //
  if (!opts.SplitMem) {
    if (opts.NoMemInitializer) {
      // TODO
      return mem;
    }

    if (!mem->hasInitializer() ||
        isa<ConstantAggregateZero>(*(mem->getInitializer()))) {

      Constant *init = createMemInitializer(llvmContext, memty,
                                            unwrapIntConstant(offset), ds.data);
      mem->setInitializer(init);
    } else { // change existing mem
      Constant *init = mem->getInitializer();
      assert(isa<ConstantDataArray>(init));
      ConstantDataArray *inita = cast<ConstantDataArray>(init);
      StringRef data = inita->getAsString();

      modMemInitializer(data, unwrapIntConstant(offset), ds.data);
    }
  } else {
    auto data_size = ds.data.size();
    auto int_offset = cast<ConstantInt>(offset);
    ArrayType *ty = ArrayType::get(Type::getInt8Ty(llvmContext), data_size);
    bool isConstant = false;

    if (removeDollar(ds.name) == ".rodata") {
      std::cerr << __FILE__ << ":" << __LINE__ << ": "
                << "Warning: Set " << ds.name
                << " data segment as constant because of its NAME."
                << std::endl;
      isConstant = true;
    }

    auto addr = int_to_hex(int_offset->getZExtValue());
    auto name = mem->getName().str() + "_" + addr;

    uint8_t *buffer = new uint8_t[data_size]; // TODO when to free?
    OwnedBuffers.push_back(buffer);
    // copy data
    ::memcpy(buffer, ds.data.data(), data_size);

    auto init =
        ConstantDataArray::getRaw(StringRef((char *)buffer, data_size),
                                  data_size, Type::getInt8Ty(llvmContext));
    auto gv = new GlobalVariable(llvmModule, ty, isConstant,
                                 GlobalValue::LinkageTypes::InternalLinkage,
                                 init, name);
    // place to a special section for later linking
    gv->setSection(".addr_" + addr);
    // byte aligned
    gv->setAlignment(llvm::Align());
  }

  return mem;
}

const char *LOCAL_PREFIX = "_local_";
const char *ARG_PREFIX = "_arg_";
const char *PARAM_PREFIX = "_param_";

void Context::visitFunc(wabt::Func &func, llvm::Function *function) {
  using namespace llvm;

  // Set that null pointer is valid for wasm funcs.
  function->addFnAttr(Attribute::get(llvmContext, Attribute::NullPointerIsValid));

  BasicBlock *allocaBlock =
      llvm::BasicBlock::Create(llvmContext, "allocator", function);

  // auto returnPHIs = createPHIs(returnBlock, func.decl.sig.result_types);

  IRBuilder<> irBuilder(llvmContext);
  irBuilder.SetInsertPoint(allocaBlock);
  // std::unique_ptr<std::vector<llvm::Value*>> locals =
  // std::make_unique<std::vector<llvm::Value*>>();
  std::vector<llvm::Value *> locals = std::vector<llvm::Value *>();

  // handle locals (params)
  Function::arg_iterator llvmArgIt = function->arg_begin();
  wabt::Index numParam = func.GetNumParams();
  for (wabt::Index i = 0; i < numParam; i++) {
    AllocaInst *alloca =
        irBuilder.CreateAlloca(convertType(llvmContext, func.GetParamType(i)),
                               nullptr, PARAM_PREFIX + std::to_string(i));
    locals.push_back(alloca);
    irBuilder.CreateStore(&*llvmArgIt, alloca);
    ++llvmArgIt;
  }
  // handle locals
  wabt::Index numLocal = func.GetNumLocals();
  for (wabt::Index i = 0; i < numLocal; i++) {
    AllocaInst *alloca = irBuilder.CreateAlloca(
        convertType(llvmContext, func.local_types[i]), nullptr,
        LOCAL_PREFIX + std::to_string(numParam + i));
    locals.push_back(alloca);
    irBuilder.CreateStore(convertZeroValue(llvmContext, func.local_types[i]),
                          alloca);
  }

  BasicBlock *returnBlock =
      llvm::BasicBlock::Create(llvmContext, "return", function);

  if (opts.LogLevel >= level_debug) {
    std::cerr << "Debug: Analyzing function " << func.name << "("
              << func.loc.filename << ":" << func.loc.line << ")" << std::endl;
  }

  BlockContext bctx(*this, *function, irBuilder, std::move(locals));
  bctx.visitBlock(wabt::LabelType::Func, allocaBlock, returnBlock, func.decl,
                  func.exprs);

  // create return
  // TODO MultiValue
  irBuilder.SetInsertPoint(returnBlock);
  if (func.GetNumResults() == 1) {
    irBuilder.CreateRet(bctx.popStack());
  } else {
    assert(func.GetNumResults() == 0); // ?
    irBuilder.CreateRetVoid();
  }
}

void Context::visitGlobal(wabt::Global &gl, bool isExternal) {
  using namespace llvm;
  std::string gname = gl.name.empty()
                          ? "__notdec_global_" + std::to_string(_glob_index)
                          : gl.name;
  if (!opts.NoRemoveDollar) {
    gname = removeDollar(gname);
  }
  Type *ty = convertType(llvmContext, gl.type);
  GlobalVariable *gv = new GlobalVariable(
      llvmModule, ty, !gl.mutable_,
      isExternal ? GlobalValue::LinkageTypes::ExternalLinkage
                 : GlobalValue::LinkageTypes::InternalLinkage,
      nullptr, gname);
  if (!isExternal) {
    Constant *init = visitInitExpr(gl.init_expr);
    if (init == nullptr) {
      // std::cerr << __FILE__ << ":" << __LINE__ << ": " << "Warn: use default
      // InitExpr." << std::endl;
      init = ConstantAggregateZero::get(ty);
    }
    gv->setInitializer(init);
  }
  this->globs.push_back(gv);
  _glob_index++;
}

// Declare the table. The initialization is done in elem section.
void Context::visitTable(wabt::Table &table, bool isExternal) {
  using namespace llvm;
  Type *ty;
  if (table.elem_type == wabt::Type::FuncRef) {
    // use a generic void func pointer, because the actual type is known.
    // maybe migrate to https://llvm.org/docs/OpaquePointers.html in the future.
    ty = getFuncPointerType();
  } else {
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: table elem type not supported: "
              << table.elem_type.GetName() << std::endl;
    std::abort();
  }

  if (table.elem_limits.has_max &&
      table.elem_limits.max != table.elem_limits.initial) {
    // TODO
    if (opts.LogLevel >= level_warning)
      std::cerr << __FILE__ << ":" << __LINE__ << ": "
                << "Warning: table elem limits has max." << std::endl;
  }

  ArrayType *aty = ArrayType::get(ty, table.elem_limits.initial);
  GlobalVariable *gv = new GlobalVariable(
      llvmModule, aty, false, GlobalValue::LinkageTypes::ExternalLinkage,
      nullptr,
      table.name.empty() ? DEFAULT_TABLE_PREFIX + std::to_string(_table_index) : table.name);
  this->tables.push_back(gv);
  _table_index++;
}

// ref: wabt/src/wat-writer.cc:1434 WatWriter::WriteElemSegment
void Context::visitElem(wabt::ElemSegment &elem) {
  using namespace llvm;
  uint8_t flags = elem.GetFlags(module.get());
  if (elem.elem_type != wabt::Type::FuncRef) {
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: elem type not supported: " << elem.elem_type.GetName()
              << std::endl;
    std::abort();
  }

  // 1 根据table index找到对应的table
  wabt::Index table_index;
  if ((flags & (wabt::SegPassive | wabt::SegExplicitIndex)) ==
      wabt::SegExplicitIndex) {
    table_index = module->GetTableIndex(elem.table_var);
  } else {
    table_index = 0;
  }
  if (flags & wabt::SegPassive) {
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: passive elem segment not supported." << std::endl;
    std::abort();
  }

  GlobalVariable *gv = tables.at(table_index); // TODO index

  // 2 把函数指针填入
  ArrayType *arr = cast<ArrayType>(gv->getValueType());
  // all null
  llvm::SmallVector<Constant *> buffer(arr->getNumElements());
  // 解析offset
  Constant *offset_constant = visitInitExpr(elem.offset);
  wabt::Index offset = unwrapIntConstant(offset_constant);
  if (offset != 0) {
    if (opts.LogLevel >= level_warning)
      std::cerr << __FILE__ << ":" << __LINE__ << ": "
                << "Warning: elem offset not zero." << std::endl;
  }
  for (wabt::Index i = 0; i < arr->getNumElements(); i++) {
    if (!(i >= offset && i < offset + elem.elem_exprs.size())) {
      buffer[i] = ConstantPointerNull::get(
          llvm::cast<PointerType>(arr->getElementType()));
      continue;
    }
    const wabt::ExprList &expr = elem.elem_exprs.at(i - offset);
    if (flags & wabt::SegUseElemExprs) {
      // TODO
      std::cerr << __FILE__ << ":" << __LINE__ << ": "
                << "Error: elem exprs not supported." << std::endl;
      std::abort();
    } else {
      assert(expr.size() == 1);
      assert(expr.front().type() == wabt::ExprType::RefFunc);
      wabt::Index func_ind =
          module->GetFuncIndex(cast<wabt::RefFuncExpr>(&expr.front())->var);
      Constant *func_addr =
          ConstantExpr::getBitCast(funcs.at(func_ind), getFuncPointerType());
      buffer[i] = func_addr;
    }
  }

  Constant *init = ConstantArray::get(arr, buffer);
  if (gv->hasInitializer()) {
    gv->getInitializer()->replaceAllUsesWith(init);
  } else {
    gv->setInitializer(init);
  }
}

llvm::Constant *Context::visitInitExpr(wabt::ExprList &expr) {
  using namespace wabt;
  if (expr.size() != 1) {
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: InitExpr size != 1." << std::endl;
    std::abort();
  }

  if (expr.front().type() == ExprType::Const) {
    const Const &const_ = cast<ConstExpr>(&expr.front())->const_;
    return visitConst(llvmContext, const_);
  } else if (expr.front().type() == ExprType::GlobalGet) {
    // TODO
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: InitExpr with global.get is currently not supported."
              << std::endl;
    std::abort();
  }
  return nullptr;
}

llvm::GlobalVariable *Context::declareMemory(wabt::Memory &mem,
                                             bool isExternal) {
  using namespace llvm;
  uint64_t len = mem.page_limits.initial;
  if (mem.page_limits.has_max) {
    uint64_t max = mem.page_limits.max;
    if (max != len) {
      std::cout << "memory min " << len << " max " << max << std::endl;
    }
    len = max;
  }
  len = len * 65536;
  ArrayType *ty = ArrayType::get(Type::getInt8Ty(llvmContext), len);
  GlobalVariable *gv = new GlobalVariable(
      llvmModule, ty, false,
      isExternal ? GlobalValue::LinkageTypes::ExternalLinkage
                 : GlobalValue::LinkageTypes::InternalLinkage,
      nullptr, "__notdec_mem0");
  if (!isExternal) {
    gv->setInitializer(ConstantAggregateZero::get(ty));
  }
  this->mems.push_back(gv);
  _mem_index++;
  return gv;
}

llvm::Function *Context::declareFunc(wabt::Func &func, bool isExternal) {
  using namespace llvm;
  FunctionType *funcType = convertFuncType(llvmContext, func.decl.sig);
  std::string fname = func.name;
  if (!opts.NoRemoveDollar) {
    fname = removeDollar(func.name);
  }
  Function *function = Function::Create(funcType,
                                        isExternal ? Function::ExternalLinkage
                                                   : Function::InternalLinkage,
                                        fname, llvmModule);
  this->funcs.push_back(function);
  this->_func_index++;
  setFuncArgName(*function, func.decl.sig);
  return function;
}

void modMemInitializer(llvm::StringRef ptr, uint64_t offset,
                       std::vector<uint8_t> data) {
  using namespace llvm;
  uint64_t memsize = ptr.size();
  assert(data.size() + offset <= memsize);
  // copy data
  ::memcpy((void *)(ptr.data() + offset), data.data(),
           data.size() * sizeof(uint8_t));
}

llvm::Constant *createMemInitializer(llvm::LLVMContext &llvmContext,
                                     llvm::Type *memty, uint64_t offset,
                                     std::vector<uint8_t> data) {
  using namespace llvm;
  uint64_t memsize = cast<ArrayType>(*memty).getNumElements();
  assert(data.size() + offset <= memsize);
  // TODO store buffer somewhere. create a struct for memory?
  uint8_t *buffer = new uint8_t[memsize]; // TODO when to free?
  memset(buffer, 0, memsize);
  GlobBuffers.push_back(buffer);
  // copy data
  ::memcpy(buffer + offset, data.data(), data.size() * sizeof(uint8_t));
  return ConstantDataArray::getRaw(StringRef((char *)buffer, memsize), memsize,
                                   Type::getInt8Ty(llvmContext));
}

std::string removeDollar(std::string name) {
  if (name.length() == 0) {
    return name;
  }
  if (name.at(0) == '$') {
    return name.substr(1);
  }
  return name;
}

llvm::StringRef removeDollar(llvm::StringRef name) {
  if (name.size() == 0) {
    return name;
  }
  if (name.front() == '$') {
    return name.substr(1);
  }
  return name;
}

void Context::setFuncArgName(llvm::Function &func,
                             const wabt::FuncSignature &decl) {
  using namespace llvm;
  wabt::Index argSize = decl.GetNumParams();
  for (wabt::Index i = 0; i < argSize; i++) {
    if (decl.param_type_names.count(i) != 0) {
      func.getArg(i)->setName(decl.param_type_names.at(i));
    } else {
      func.getArg(i)->setName(ARG_PREFIX + std::to_string(i));
    }
    // std::cout << func.getArg(i)->getName().str() << std::endl;
  }
}

llvm::Function *Context::findFunc(wabt::Var &var) {
  wabt::Index ind;
  if (var.is_index()) {
    ind = var.index();
  } else {
    ind = module->func_bindings.FindIndex(var);
  }
  return funcs.at(ind);
}

llvm::FunctionType *convertFuncType(llvm::LLVMContext &llvmContext,
                                    const wabt::FuncSignature &decl) {
  using namespace llvm;
  Type **llvmArgTypes;
  size_t numParameters = decl.param_types.size();
  llvmArgTypes = (Type **)alloca(sizeof(Type *) * numParameters);
  int i = 0;
  for (const wabt::Type &pt : decl.param_types) {
    llvmArgTypes[i] = convertType(llvmContext, pt);
    i++;
  }
  // TODO multi return
  if (decl.GetNumResults() > 1) {
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: Multi result is currently not supported." << std::endl;
    std::abort();
  }
  Type *llvmReturnType = convertReturnType(llvmContext, decl);
  return FunctionType::get(
      llvmReturnType, ArrayRef<Type *>(llvmArgTypes, numParameters), false);
}

llvm::Type *convertReturnType(llvm::LLVMContext &llvmContext,
                              const wabt::FuncSignature &decl) {
  using namespace llvm;
  Type *llvmReturnType;
  if (decl.GetNumResults() == 0) {
    llvmReturnType = Type::getVoidTy(llvmContext);
  } else {
    llvmReturnType = convertType(llvmContext, decl.GetResultType(0));
  }
  return llvmReturnType;
}

// TODO 从类里提出来
llvm::Type *convertType(llvm::LLVMContext &llvmContext, const wabt::Type &ty) {
  using namespace llvm;
  switch (ty) {
  case wabt::Type::I32:
    return Type::getInt32Ty(llvmContext);
  case wabt::Type::I64:
    return Type::getInt64Ty(llvmContext);
  case wabt::Type::F32:
    return Type::getFloatTy(llvmContext);
  case wabt::Type::F64:
    return Type::getDoubleTy(llvmContext);
  case wabt::Type::V128:
    return Type::getInt128Ty(llvmContext);
  case wabt::Type::Void:
    return Type::getVoidTy(llvmContext);
  default:
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: Cannot convert type: " << ty.GetName() << std::endl;
    std::abort();
  }
}

llvm::Constant *convertZeroValue(llvm::LLVMContext &llvmContext,
                                 const wabt::Type &ty) {
  using namespace llvm;
  switch (ty) {
  case wabt::Type::I32:
    return ConstantInt::get(Type::getInt32Ty(llvmContext), 0);
  case wabt::Type::I64:
    return ConstantInt::get(Type::getInt64Ty(llvmContext), 0);
  case wabt::Type::F32:
    return llvm::ConstantFP::get(Type::getFloatTy(llvmContext), 0);
  case wabt::Type::F64:
    return llvm::ConstantFP::get(Type::getDoubleTy(llvmContext), 0);
  case wabt::Type::V128:
    return ConstantInt::get(Type::getInt128Ty(llvmContext), 0);
  case wabt::Type::Void:
  default:
    std::cerr << __FILE__ << ":" << __LINE__ << ": "
              << "Error: Cannot convert type: " << ty.GetName() << std::endl;
    std::abort();
  }
}
} // namespace notdec::frontend::wasm

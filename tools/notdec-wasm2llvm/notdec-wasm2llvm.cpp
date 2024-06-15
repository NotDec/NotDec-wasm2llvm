#include <iostream>
#include <memory>
#include <string>

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include "parser.h"
#include "utils.h"

using namespace llvm;
using namespace notdec::frontend::wasm;

static cl::opt<std::string> inputFilename(
    cl::Positional, cl::desc("<input file>"),
    cl::value_desc(
        "input WebAssembly bytecode file, either .wasm or .wat path."),
    cl::Required);

static cl::opt<std::string> outputPath("o", cl::desc("Specify output path"),
                                       cl::value_desc("output.ll"),
                                       cl::Optional);

cl::opt<log_level>
    logLevel("log-level", cl::desc("Log level:"),
             cl::values(clEnumValN(level_emergent, "emergent", "emergent"),
                        clEnumValN(level_alert, "alert", "alert"),
                        clEnumValN(level_critical, "critical", "critical"),
                        clEnumValN(level_error, "error", "error"),
                        clEnumValN(level_warning, "warning", "warning"),
                        clEnumValN(level_notice, "notice", "notice"),
                        clEnumValN(level_info, "info", "info"),
                        clEnumValN(level_debug, "debug", "debug")),
             cl::init(level_notice));

static cl::opt<bool>
    GenIntToPtr("gen-int-to-ptr",
                cl::desc("Generate inttoptr for memory access, instead of get "
                         "element ptr of the global memory"),
                cl::init(false));

static cl::opt<bool>
    FixNames("fix-names",
             cl::desc("apply some name transformations, e.g. transform the "
                      "__original_main/__main_argc_argv to main"),
             cl::init(false));

static cl::opt<bool>
    NoRemoveDollar("no-remove-dollar",
                   cl::desc("do not remove dollar '$' prefix from the name."),
                   cl::init(false));

static cl::opt<bool> ForceExportName(
    "force-export-name",
    cl::desc("Rename function to its export name even if it has a name"),
    cl::init(false));

static cl::opt<bool>
    SplitMem("split-mem",
             cl::desc("(Breaks execution!) Split initialized parts of the "
                      "memory to global variables."),
             cl::init(false));

static cl::opt<bool> NoMemInitializer(
    "no-mem-initializer",
    cl::desc("(Breaks execution!) Do not generate memory initializer, because "
             "currently the initializer is flattened, which makes the bytecode "
             "really big in size."),
    cl::init(false));

std::string getSuffix(std::string fname) {
  std::size_t ind = fname.find_last_of('.');
  if (ind != std::string::npos) {
    return fname.substr(ind);
  }
  return std::string();
}

// https://llvm.org/docs/ProgrammersManual.html#the-llvm-debug-macro-and-debug-option
// initialize function for the fine-grained debug info with DEBUG_TYPE and the
// -debug-only option
namespace llvm {
void initDebugOptions();
}

int main(int argc, char *argv[]) {
  // parse cmdline
  cl::ParseCommandLineOptions(
      argc, argv,
      "NotDec llvm to C decompiler frontend: Translates "
      "WebAssembly into LLVM IR.\n");
  // initDebugOptions();

  std::string inSuffix = getSuffix(inputFilename);
  llvm::LLVMContext Ctx;
  std::unique_ptr<llvm::Module> mod;
  std::unique_ptr<Context> Context;
  Options Opts = {
      .GenIntToPtr = GenIntToPtr,
      .FixNames = FixNames,
      .NoRemoveDollar = NoRemoveDollar,
      .ForceExportName = ForceExportName,
      .SplitMem = SplitMem,
      .NoMemInitializer = NoMemInitializer,
      .LogLevel = logLevel,
  };
  if (inSuffix.size() == 0) {
    std::cout << "no extension for input file. exiting." << std::endl;
    return 0;
  } else if (inSuffix == ".wasm") {
    std::cout << "Loading Wasm: " << inputFilename << std::endl;
    SMDiagnostic Err;
    mod = std::make_unique<llvm::Module>(outputPath, Ctx);
    Context = parse_wasm(Ctx, *mod, Opts, inputFilename);
  } else if (inSuffix == ".wat") {
    std::cout << "Loading Wat: " << inputFilename << std::endl;
    SMDiagnostic Err;
    mod = std::make_unique<llvm::Module>(outputPath, Ctx);
    Context = parse_wat(Ctx, *mod, Opts, inputFilename);
  } else {
    std::cout << "unknown extension " << inSuffix << " for input file. exiting."
              << std::endl;
    return 0;
  }

  std::string outSuffix = getSuffix(outputPath);
  if (outSuffix == ".ll") {
    std::error_code EC;
    llvm::raw_fd_ostream os(outputPath, EC);
    if (EC) {
      std::cerr << "Cannot open output file." << std::endl;
      std::cerr << EC.message() << std::endl;
      std::abort();
    }
    mod->print(os, nullptr);
    std::cerr << "IR dumped to " << outputPath << std::endl;
  } else if (outSuffix == ".bc") {
    std::error_code EC;
    llvm::raw_fd_ostream os(outputPath, EC);
    if (EC) {
      std::cerr << "Cannot open output file." << std::endl;
      std::cerr << EC.message() << std::endl;
      std::abort();
    }
    WriteBitcodeToFile(*mod, os);
    std::cerr << "Bitcode dumped to " << outputPath << std::endl;
  } else {
    std::cerr << "Unknown suffix for path: " << outputPath << std::endl;
  }

  return 0;
}

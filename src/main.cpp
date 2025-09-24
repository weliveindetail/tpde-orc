#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>

using namespace llvm;
using namespace llvm::orc;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input IR file>"), cl::Required);

static cl::opt<std::string> EntryPoint("entrypoint",
                                       cl::desc("Entry point function name"),
                                       cl::init("main"));

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "TPDE ORC JIT Compiler\n");

  SMDiagnostic Err;
  std::unique_ptr<LLVMContext> Context = std::make_unique<LLVMContext>();
  auto Mod = parseIRFile(InputFilename, Err, *Context);
  if (!Mod) {
    Err.print("tpde-orc", errs());
    return 1;
  }

  outs() << "Loaded module: " << Mod->getName() << "\n";

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  ExitOnError ExitOnErr;
  auto Builder = LLJITBuilder();
  std::unique_ptr<LLJIT> JIT = ExitOnErr(Builder.create());

  ThreadSafeModule TSM(std::move(Mod), std::move(Context));
  ExitOnErr(JIT->addIRModule(std::move(TSM)));

  auto EntryAddr = ExitOnErr(JIT->lookup(EntryPoint));
  auto EntryFn = EntryAddr.toPtr<int (*)()>();
  outs() << "Executing " << EntryPoint << "()\n";

  int Result = EntryFn();
  outs() << "Program returned: " << Result << "\n";

  return 0;
}

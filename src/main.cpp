#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include "tpde-llvm/LLVMCompiler.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::orc;

static cl::opt<std::string> Input(cl::Positional, cl::desc("<input IR file>"),
                                  cl::Required);

static cl::opt<std::string> EntryPoint("entrypoint",
                                       cl::desc("Entry point function name"),
                                       cl::init("main"));

static cl::opt<unsigned>
    Threads("par", cl::desc("Compile csmith code on N threads concurrently"),
            cl::init(0));

static cl::opt<unsigned>
    Duplicates("dups", cl::desc("Duplicates to increase concurrent workload"),
               cl::init(100));

static cl::opt<bool> UseLLVM("llvm",
                             cl::desc("Use LLVM's built-in backend, not TPDE"),
                             cl::init(false));
ExitOnError ExitOnErr;
class TPDECompiler : public IRCompileLayer::IRCompiler {
public:
  TPDECompiler(JITTargetMachineBuilder JTMB)
      : IRCompiler(irManglingOptionsFromTargetOptions(JTMB.getOptions())),
        JTMB(std::move(JTMB)) {
    assert(Compiler != nullptr && "Unknown architecture");
  }

  Expected<std::unique_ptr<MemoryBuffer>> operator()(Module &M) override {
    std::vector<uint8_t> *B;
    {
      std::lock_guard<std::mutex> Lock(BuffersAccess);
      Buffers.push_back(std::make_unique<std::vector<uint8_t>>());
      B = Buffers.back().get();
    }

    if (!Compiler->compile_to_elf(M, *B)) {
#ifndef NDEBUG
      errs() << "Falling back to LLVM for module: " << M.getName() << "\n";
#endif
      auto TM = ExitOnErr(JTMB.createTargetMachine());
      return SimpleCompiler(*TM)(M);
    }

    StringRef BufferRef{reinterpret_cast<char *>(B->data()), B->size()};
    return MemoryBuffer::getMemBuffer(BufferRef, "", false);
  }

private:
  static thread_local std::unique_ptr<tpde_llvm::LLVMCompiler> Compiler;
  std::vector<std::unique_ptr<std::vector<uint8_t>>> Buffers;
  std::mutex BuffersAccess;
  JITTargetMachineBuilder JTMB;
};

thread_local std::unique_ptr<tpde_llvm::LLVMCompiler> TPDECompiler::Compiler =
    tpde_llvm::LLVMCompiler::create(Triple(LLVM_HOST_TRIPLE));

ThreadSafeModule loadModule(LLJIT &JIT) {
  SMDiagnostic Err;
  std::unique_ptr<LLVMContext> Context = std::make_unique<LLVMContext>();
  outs() << "Load module: " << Input << "\n";
  auto Mod = parseIRFile(Input, Err, *Context);
  if (!Mod) {
    errs() << "Failed to parse file: " << Input << "\n";
    Err.print("tpde-orc", errs());
    exit(1);
  }
  return ThreadSafeModule(std::move(Mod), std::move(Context));
}

ThreadSafeModule cloneModule(LLJIT &JIT, MemoryBufferRef Bitcode,
                             StringRef Name) {
  ValueToValueMapTy VMap;
  auto Ctx = std::make_unique<LLVMContext>();
  auto NewCtx = std::make_unique<LLVMContext>();
  auto NewMod = ExitOnErr(parseBitcodeFile(Bitcode, *NewCtx));
  NewMod->setModuleIdentifier(Name);
  Function *F = NewMod->getFunction("main");
  F->setName(Name);
  return ThreadSafeModule(std::move(NewMod), std::move(NewCtx));
}

SymbolLookupSet addDuplicates(LLJIT &JIT, Module &Mod) {
  SmallVector<char, 0> Buffer;
  {
    raw_svector_ostream OS(Buffer);
    WriteBitcodeToFile(Mod, OS);
  }
  MemoryBufferRef Bitcode(StringRef(Buffer.data(), Buffer.size()), "");

  // Generate dupes with distinct entry-points
  SymbolLookupSet EntryPoints;
  for (size_t c = 0; c < Duplicates; c += 1) {
    std::string Name;
    std::filesystem::path FileName = std::string(Input);
    raw_string_ostream(Name) << FileName.stem() << "_" << c;
    auto Clone = cloneModule(JIT, Bitcode, Name);
    if (Error Err = JIT.addIRModule(std::move(Clone))) {
      errs() << "Skip module " << Name << ": " << std::move(Err) << "\n";
    } else {
      EntryPoints.add(JIT.mangleAndIntern(Name));
    }
  }
  return EntryPoints;
}

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "TPDE ORC JIT Compiler\n");

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  auto Builder = LLJITBuilder();
  if (!UseLLVM)
    Builder.CreateCompileFunction = [](JITTargetMachineBuilder JTMB)
        -> Expected<std::unique_ptr<IRCompileLayer::IRCompiler>> {
      return std::make_unique<TPDECompiler>(JTMB);
    };
  if (Threads > 1) {
    Builder.SupportConcurrentCompilation = true;
    Builder.NumCompileThreads = Threads;
  }
  std::unique_ptr<LLJIT> JIT = ExitOnErr(Builder.create());

  ThreadSafeModule Mod = loadModule(*JIT);
  if (Threads == 0) {
    ExitOnErr(JIT->addIRModule(std::move(Mod)));
    auto Addr = ExitOnErr(JIT->lookup(EntryPoint));
    auto Fn = Addr.toPtr<int (*)()>();
    outs() << "Executing " << EntryPoint << "()\n";
    int Result = Fn();
    outs() << "Program returned: " << Result << "\n";
    return 0;
  }

  SymbolLookupSet EntryPoints = addDuplicates(*JIT, *Mod.getModuleUnlocked());

  // Compile in parallel and invoke all entry-points
  outs() << "Compiling " << EntryPoints.size() << " modules on " << Threads
         << " threads in parallel\n";
  auto SO = makeJITDylibSearchOrder({&JIT->getMainJITDylib()});
  auto &ES = JIT->getExecutionSession();

  SymbolMap SymMap;
  auto Start = std::chrono::steady_clock::now();
  SymMap = ExitOnErr(ES.lookup(SO, EntryPoints));
  auto End = std::chrono::steady_clock::now();
  auto Elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(End - Start);

  outs() << "Executing all entrypoints\n";
  for (auto [Name, Addr] : SymMap) {
    outs() << "Executing " << Name << "() ";
    auto Fn = Addr.getAddress().toPtr<int (*)()>();
    Fn();
  }

  outs() << "Compile-time was: " << Elapsed.count() << " ms\n";
  return 0;
}

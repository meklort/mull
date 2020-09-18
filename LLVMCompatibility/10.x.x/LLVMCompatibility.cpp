#include "LLVMCompatibility.h"

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/CommandLine.h>

using namespace llvm;

namespace llvm_compat {

JITSymbolFlags JITSymbolFlagsFromObjectSymbol(const object::BasicSymbolRef &symbol) {
  auto symbolOrError = JITSymbolFlags::fromObjectSymbol(symbol);
  if (symbolOrError) {
    return symbolOrError.get();
  }
  return JITSymbolFlags();
}

uint64_t JITSymbolAddress(JITSymbol &symbol) {
  auto addressOrError = symbol.getAddress();
  if (!addressOrError) {
    consumeError(addressOrError.takeError());
    return 0;
  }

  return addressOrError.get();
}

object::OwningBinary<object::ObjectFile> compileModule(orc::SimpleCompiler &compiler,
                                                       llvm::Module &module) {
  auto buffer = compiler(module);
  if (!buffer) {
    consumeError(buffer.takeError());
  }
  auto bufferRef = (*buffer)->getMemBufferRef();
  auto objectOrError = object::ObjectFile::createObjectFile(bufferRef);
  if (!objectOrError) {
    consumeError(objectOrError.takeError());
    return object::OwningBinary<object::ObjectFile>();
  }

  return object::OwningBinary<object::ObjectFile>(std::move(objectOrError.get()),
                                                  std::move(*buffer));
}

std::unique_ptr<Module> parseBitcode(MemoryBufferRef bufferRef, LLVMContext &context) {
  auto module = parseBitcodeFile(bufferRef, context);
  if (!module) {
    logAllUnhandledErrors(module.takeError(), errs(), "\nparseBitcodeFile failed: ");
    return std::unique_ptr<Module>();
  }

  return std::move(module.get());
}

void setVersionPrinter(void (*oldPrinter)(), void (*newPrinter)(raw_ostream &)) {
  llvm::cl::SetVersionPrinter(newPrinter);
}

StringRef getSectionContent(const object::SectionRef &section) {
  Expected<StringRef> content = section.getContents();
  if (!content) {
    return {};
  }
  return content.get();
}

DICompileUnit *getUnit(const DebugLoc &debugLocation) {
  DIScope *scope = debugLocation->getScope();
  while (!llvm::isa<llvm::DISubprogram>(scope) && scope != nullptr) {
    scope = scope->getScope();
  }
  return scope ? llvm::cast<llvm::DISubprogram>(scope)->getUnit() : nullptr;
}

bool parseCommandLineOptions(int argc, char **argv) {
  return llvm::cl::ParseCommandLineOptions(argc, argv, "", &llvm::errs());
}

} // namespace llvm_compat

#define DEBUG_TYPE "cfcss-instruction-index"

#include "InstructionIndex.h"

#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static const char *debugPrefix = "InstructionIndex: ";

namespace cfcss {

  InstructionIndex::InstructionIndex() : ModulePass(ID) {

  }


  void InstructionIndex::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }


  bool InstructionIndex::runOnModule(Module &M) {
    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      if (fi->isDeclaration()) {
        continue;
      }

      DEBUG(errs() << debugPrefix << "Running on [" << fi->getName() << "] ... ");

      CallList *callList = new CallList();
      callsByFunction.insert(std::pair<Function*, CallList*>(fi, callList));

      PrimaryCallMap *primaryCalls = new PrimaryCallMap();
      primaryCallsByFunction.insert(std::pair<Function*, PrimaryCallMap*>(fi, primaryCalls));

      ReturnList *returnList = new ReturnList();
      returnsByFunction.insert(std::pair<Function*, ReturnList*>(fi, returnList));

      for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
          if (CallInst *callInst = dyn_cast<CallInst>(ii)) {
            if (Function *calledFunction = callInst->getCalledFunction()) {
              if (!calledFunction->isDeclaration() && !calledFunction->isIntrinsic()) {
                callList->push_back(callInst);

                if (!primaryCalls->count(calledFunction)) {
                  primaryCalls->insert(std::pair<Function*, CallInst*>(calledFunction, callInst));
                }
              }
            } else {
              // TODO(hermannloose): Handle function pointers & inline asm.
            }
          }

          if (ReturnInst *returnInst = dyn_cast<ReturnInst>(ii)) {
            returnList->push_back(returnInst);
          }
        }
      }

      DEBUG(
        errs().changeColor(raw_ostream::GREEN);
        errs() << "done\n";
        errs().resetColor();
      );
    }

    return false;
  }


  CallList* InstructionIndex::getCalls(Function * const F) {
    return callsByFunction.lookup(F);
  }


  CallInst* InstructionIndex::getPrimaryCallTo(Function * const target,
      Function * const container) {

    PrimaryCallMap *primaryCalls = primaryCallsByFunction.lookup(container);

    return primaryCalls->lookup(target);
  }


  bool InstructionIndex::doesNotReturn(Function * const F) {
    return F->doesNotReturn() || returnsByFunction.lookup(F)->empty();
  }


  ReturnList* InstructionIndex::getReturns(Function * const F) {
    return returnsByFunction.lookup(F);
  }


  ReturnInst* InstructionIndex::getPrimaryReturn(Function * const F) {
    ReturnList* returns = returnsByFunction.lookup(F);
    assert(!returns->empty());

    return returns->front();
  }


  char InstructionIndex::ID = 0;
}

static RegisterPass<cfcss::InstructionIndex> X("instruction-index", "Instruction Index (CFCSS)");

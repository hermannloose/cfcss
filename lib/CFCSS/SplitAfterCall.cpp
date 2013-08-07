/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "SplitAfterCall.h"

#include "GatewayFunctions.h"
#include "RemoveCFGAliasing.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

static const char *debugPrefix = "SplitAfterCall: ";

namespace cfcss {

  STATISTIC(NumBlocksSplit, "Number of basic blocks split after call instructions");

  typedef std::pair<BasicBlock*, Function*> BlockToFunctionEntry;

  SplitAfterCall::SplitAfterCall() : ModulePass(ID), ignoreBlocks(), afterCall(),
      returnFromCallTo() {}

  void SplitAfterCall::getAnalysisUsage(AnalysisUsage &AU) const {
    // TODO(hermannloose): AU.setPreservesAll() would probably not hurt.
    AU.addPreserved<CallGraph>();
    AU.addPreserved<GatewayFunctions>();
    AU.addPreserved<RemoveCFGAliasing>();
  }

  bool SplitAfterCall::runOnModule(Module &M) {
    bool modifiedCFG = false;

    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      if (fi->isDeclaration()) {
        DEBUG(errs() << debugPrefix << "Skipping [" << fi->getName() << "], is a declaration.\n");
        continue;
      }

      DEBUG(errs() << debugPrefix << "Running on [" << fi->getName() << "].\n");

      for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
        // FIXME(hermannloose): This might not be needed.
        if (ignoreBlocks.count(bi)) {
          continue;
        }

        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
          if (CallInst *callInst = dyn_cast<CallInst>(ii)) {
            DEBUG(errs() << debugPrefix << "Found CallInst in [" << bi->getName() << "]:\n");
            DEBUG(ii->dump());

            if (Function *calledFunction = callInst->getCalledFunction()) {
              if (!calledFunction->isDeclaration()) {
                // Don't let our iterator wander off into the split block.
                BasicBlock::iterator nextInst(ii);
                ++nextInst;

                // TODO(hermannloose): Verify that the tail of the split block
                // will always be iterated over, to handle the case of multiple
                // call instructions in a single basic block.
                BasicBlock *afterCallBlock = llvm::SplitBlock(bi, nextInst, this);
                afterCall.insert(afterCallBlock);
                returnFromCallTo.insert(BlockToFunctionEntry(afterCallBlock, calledFunction));
                ignoreBlocks.insert(bi);

                ++NumBlocksSplit;
                modifiedCFG = true;
              } else {
                // We won't have signatures for those functions.
                DEBUG(errs() << debugPrefix << "Called function is a declaration, skipping.\n");
              }
            } else {
              // We can't handle function pointers, inline assembly, etc.
              DEBUG(errs() << debugPrefix << "Not a direct function call, skipping.\n");
            }
          }
        }
      }

      DEBUG(errs() << debugPrefix << "Finished on [" << fi->getName() << "].\n");
    }

    return modifiedCFG;
  }


  bool SplitAfterCall::wasSplitAfterCall(BasicBlock * const BB) {
    return afterCall.count(BB);
  }


  Function* SplitAfterCall::getCalledFunctionForReturnBlock(BasicBlock * const BB) {
    return returnFromCallTo.lookup(BB);
  }

  char SplitAfterCall::ID = 0;
}

static RegisterPass<cfcss::SplitAfterCall> X("split-after-call", "Split After Call (CFCSS)");

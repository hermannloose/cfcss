/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "SplitAfterCall.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

namespace cfcss {

  STATISTIC(NumBlocksSplit, "Number of basic blocks split after call instructions");

  SplitAfterCall::SplitAfterCall() : FunctionPass(ID), ignoreBlocks() {}

  void SplitAfterCall::getAnalysisUsage(AnalysisUsage &AU) const {
    // TODO(hermannloose): Which ones do we preserve, actually?
  }

  bool SplitAfterCall::runOnFunction(Function &F) {
    bool modifiedCFG = false;

    for (Function::iterator bi = F.begin(), be = F.end(); bi != be; ++bi) {
      if (ignoreBlocks.count(bi)) {
        DEBUG(errs() << "Ignoring [" << bi->getName() << "].\n");
        continue;
      }
      DEBUG(errs() << "Inspecting [" << bi->getName() << "].\n");

      for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        DEBUG(errs() << "Inspecting instruction ...\n");
        DEBUG(ii->dump());
        if (isa<CallInst>(ii)) {
          DEBUG(errs() << "Found CallInst in [" << bi->getName() << "]:\n");
          DEBUG(ii->dump());

          // Don't let our iterator wander off into the split block.
          BasicBlock::iterator nextInst(ii);
          ++nextInst;

          BasicBlock *newBlock = llvm::SplitBlock(bi, nextInst, this);
          ignoreBlocks.insert(bi);
          DEBUG(errs() << "Split basic block.\n");
          DEBUG(bi->dump());
          DEBUG(newBlock->dump());

          ++NumBlocksSplit;
          modifiedCFG = true;
        }
      }
    }

    return modifiedCFG;
  }

  char SplitAfterCall::ID = 0;
}

static RegisterPass<cfcss::SplitAfterCall>
    X("split-after-call", "Split basic blocks after call instructions (CFCSS)", false, false);

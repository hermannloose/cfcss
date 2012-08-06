/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "SplitAfterCall.h"

#include "RemoveCFGAliasing.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

static const char *debugPrefix = "SplitAfterCall: ";

namespace cfcss {

  STATISTIC(NumBlocksSplit, "Number of basic blocks split after call instructions");

  SplitAfterCall::SplitAfterCall() : FunctionPass(ID), ignoreBlocks() {}

  void SplitAfterCall::getAnalysisUsage(AnalysisUsage &AU) const {
    // TODO(hermannloose): What do we preserve?
    AU.addPreserved<RemoveCFGAliasing>();
  }

  bool SplitAfterCall::runOnFunction(Function &F) {
    if (F.isDeclaration()) {
      DEBUG(errs() << debugPrefix << "Skipping [" << F.getName() << "], is a declaration.\n");
      return false;
    }

    DEBUG(errs() << debugPrefix << "Running on [" << F.getName() << "].\n");
    bool modifiedCFG = false;

    for (Function::iterator bi = F.begin(), be = F.end(); bi != be; ++bi) {
      // FIXME(hermannloose): This might not be needed.
      if (ignoreBlocks.count(bi)) {
        //DEBUG(errs() << "Ignoring [" << bi->getName() << "].\n");
        continue;
      }
      //DEBUG(errs() << "Inspecting [" << bi->getName() << "].\n");

      for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        if (isa<CallInst>(ii)) {
          DEBUG(errs() << debugPrefix << "Found CallInst in [" << bi->getName() << "]:\n");
          DEBUG(ii->dump());

          // Don't let our iterator wander off into the split block.
          BasicBlock::iterator nextInst(ii);
          ++nextInst;

          llvm::SplitBlock(bi, nextInst, this);
          ignoreBlocks.insert(bi);

          ++NumBlocksSplit;
          modifiedCFG = true;
        }
      }
    }

    DEBUG(errs() << debugPrefix << "Finished on [" << F.getName() << "].\n");
    return modifiedCFG;
  }

  char SplitAfterCall::ID = 0;
}

static RegisterPass<cfcss::SplitAfterCall>
    X("split-after-call", "Split basic blocks after call instructions (CFCSS)", true, true);

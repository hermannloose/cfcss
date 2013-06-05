/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {
  // FIXME(hermannloose): Remove duplication & is 64 sensible?
  typedef SmallPtrSet<BasicBlock*, 64> BlockSet;
  typedef DenseMap<BasicBlock*, Function*> BlockToFunctionMap;

  /**
   * Split basic blocks after call instructions.
   *
   * Call instructions don't terminate basic blocks in LLVM but represent
   * non-sequential control flow. In order to treat the remaining code in those
   * basic blocks like a normal basic block that control flow can arrive at and
   * where signatures have to be checked, we split them for easier handling.
   */
  class SplitAfterCall : public ModulePass {
    public:
      static char ID;

      SplitAfterCall();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module &M);

      /**
       * Check whether the given basic block is the remainder of another basic
       * block that was split after a call instruction.
       */
      bool wasSplitAfterCall(BasicBlock * const BB);

      /**
       * Get the function that we just returned from when entering the given
       * basic block. This implies that wasSplitAfterCall(BB) is true.
       */
      Function* getCalledFunctionForReturnBlock(BasicBlock * const BB);

    private:
      BlockSet ignoreBlocks;
      BlockSet afterCall;
      BlockToFunctionMap returnFromCallTo;
  };

}

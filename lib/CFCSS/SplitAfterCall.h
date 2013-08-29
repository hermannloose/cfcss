#pragma once

#include "Common.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace cfcss {

  /**
   * Split basic blocks after call instructions.
   *
   * Call instructions don't terminate basic blocks in LLVM but represent non-sequential control
   * flow. In order to treat the remaining code in those basic blocks like a normal basic block
   * that control flow can arrive at and where signatures have to be checked, we split them for
   * easier handling.
   */
  class SplitAfterCall : public llvm::ModulePass {
    public:
      static char ID;

      SplitAfterCall();

      virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
      virtual bool runOnModule(llvm::Module &M);

      /**
       * Check whether the given basic block is the remainder of another basic block that was split
       * after a call instruction.
       */
      bool wasSplitAfterCall(llvm::BasicBlock * const BB);

      /**
       * Get the function that we just returned from when entering the given basic block.
       *
       * This implies that wasSplitAfterCall(BB) is true.
       */
      llvm::Function* getCalledFunctionForReturnBlock(llvm::BasicBlock * const BB);

    private:
      BlockSet ignoreBlocks;
      BlockSet afterCall;
      BlockToFunctionMap returnFromCallTo;
  };

}

/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {
  // FIXME(hermannloose): Remove duplication & is 64 sensible?
  typedef SmallPtrSet<BasicBlock*, 64> BlockSet;

  class SplitAfterCall : public ModulePass {
    public:
      static char ID;

      SplitAfterCall();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module &M);

      bool wasSplitAfterCall(BasicBlock * const BB);

    private:
      BlockSet ignoreBlocks;
      BlockSet afterCall;
  };

}

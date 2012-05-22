/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {

  class RemoveCFGAliasing : public FunctionPass {
    public:
      typedef SmallPtrSet<BasicBlock*, 32> BlockSet;
      typedef DenseMap<BasicBlock*, BlockSet*> BlockMap;

      static char ID;

      RemoveCFGAliasing();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnFunction(Function &F);

    private:
      BlockMap* getAliasingBlocks(BasicBlock *BB);
  };

}

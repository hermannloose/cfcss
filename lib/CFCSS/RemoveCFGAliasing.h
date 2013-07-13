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

  /**
   * Remove cases of fanin nodes in the CFG aliasing by inserting proxy blocks
   * on all offending edges.
   */
  class RemoveCFGAliasing : public ModulePass {
    public:
      typedef SmallPtrSet<BasicBlock*, 32> BlockSet;
      typedef DenseMap<BasicBlock*, BlockSet*> BlockMap;

      static char ID;

      RemoveCFGAliasing();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module &M);

    private:
      BlockMap* getAliasingBlocks(BasicBlock *BB);
      BasicBlock* insertProxyBlock(BasicBlock *source, BasicBlock *target);
  };

}

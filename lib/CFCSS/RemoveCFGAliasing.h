/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "Common.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace cfcss {

  /**
   * Remove cases of fanin nodes in the CFG aliasing by inserting proxy blocks
   * on all offending edges.
   */
  class RemoveCFGAliasing : public llvm::ModulePass {
    public:
      static char ID;

      RemoveCFGAliasing();

      virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
      virtual bool runOnModule(llvm::Module &M);

    private:
      BlockToBlockSetMap* getAliasingBlocks(llvm::BasicBlock *BB);
      llvm::BasicBlock* insertProxyBlock(llvm::BasicBlock *source, llvm::BasicBlock *target);
  };

}

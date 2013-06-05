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
  typedef DenseMap<Function*, BlockSet*> ReturnBlockMap;
  typedef DenseMap<Function*, BasicBlock*> FunctionToBlockMap;

  class ReturnBlocks : public ModulePass {
    public:
      static char ID;

      ReturnBlocks();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module &M);

      BlockSet* getReturnBlocks(Function * const F);
      BasicBlock* getAuthoritativeReturnBlock(Function * const F);

    private:
      ReturnBlockMap returnBlocks;
      FunctionToBlockMap authoritativeReturnBlocks;
  };

}

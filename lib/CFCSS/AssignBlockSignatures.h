/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {
  typedef DenseMap<BasicBlock*, ConstantInt*> SignatureMap;
  typedef DenseMap<BasicBlock*, BasicBlock*> BlockMap;
  // TODO(hermannloose): Is 64 a sensible value?
  typedef SmallPtrSet<BasicBlock*, 64> BlockSet;

  class AssignBlockSignatures : public ModulePass {
    public:
      static char ID;

      AssignBlockSignatures();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module &M);

      ConstantInt* getSignature(BasicBlock * const BB);
      bool isFaninNode(BasicBlock * const BB);
      bool hasFaninSuccessor(BasicBlock * const BB);

      BasicBlock* getAuthoritativePredecessor(BasicBlock * const BB);
      BasicBlock* getAuthoritativeSibling(BasicBlock * const BB);

      void notifyAboutSplitBlock(BasicBlock * const head,
          BasicBlock * const tail);

    private:
      SignatureMap blockSignatures;
      SignatureMap signatureUpdateSources;
      BlockMap adjustFor;
      BlockSet faninBlocks;
      BlockSet faninSuccessors;
      unsigned long nextID;
  };

}

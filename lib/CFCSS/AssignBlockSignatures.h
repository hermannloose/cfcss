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

  /**
   * Assign signatures to every basic block in a module and provide these
   * signatures keyed by basic block for later passes.
   */
  class AssignBlockSignatures : public ModulePass {
    public:
      static char ID;

      AssignBlockSignatures();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module &M);

      /**
       * Get the signature of the given basic block, if any.
       */
      ConstantInt* getSignature(BasicBlock * const BB);

      /**
       * Check whether the given basic block is a fanin node.
       */
      bool isFaninNode(BasicBlock * const BB);

      /**
       * Check whether the given basic block has a successor that is a fanin
       * node.
       */
      bool hasFaninSuccessor(BasicBlock * const BB);

      /**
       * Get the authoritative predecessor of the given basic block.
       *
       * The authoritative predecessor dictates the signature difference (XOR
       * between two block signatures) used in computing the updated signature
       * upon entering a basic block and is used during instrumentation.
       */
      BasicBlock* getAuthoritativePredecessor(BasicBlock * const BB);

      /**
       * Get the authoritative sibling of the given basic block.
       *
       * In the case of fanin nodes, siblings of the fanin node's authoritative
       * predecessor have to set the runtime adjusting signature D to the
       * signature difference between them and the authoritative predecessor,
       * i.e. their authoritative sibling. This is used in instrumentation.
       */
      BasicBlock* getAuthoritativeSibling(BasicBlock * const BB);

      /**
       * Give the tail of a split block the same authoritative predecessor as
       * the head.
       *
       * FIXME(hermannloose): This is clumsy and I'm not even sure it's needed.
       */
      void notifyAboutSplitBlock(BasicBlock * const head,
          BasicBlock * const tail);

    private:
      SignatureMap blockSignatures;
      SignatureMap signatureUpdateSources;
      BlockMap adjustFor;
      BlockSet faninBlocks;
      // TODO(hermannloose): Rename this, since it's misleading.
      BlockSet faninSuccessors;
      unsigned long nextID;
  };

}

#pragma once

#include "Common.h"

#include "llvm/Pass.h"

namespace cfcss {

  /**
   * Assign signatures to every basic block in a module and provide these signatures keyed by basic
   * block for later passes.
   */
  class AssignBlockSignatures : public llvm::ModulePass {
    public:
      static char ID;

      AssignBlockSignatures();

      virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
      virtual bool runOnModule(llvm::Module &M);

      /**
       * Get the signature of the given basic block, if any.
       */
      Signature* getSignature(llvm::BasicBlock * const BB);

      /**
       * Check whether the given basic block is a fanin node.
       */
      bool isFaninNode(llvm::BasicBlock * const BB);

      /**
       * Check whether the given basic block has a successor that is a fanin node.
       */
      bool hasFaninSuccessor(llvm::BasicBlock * const BB);

      /**
       * Get the authoritative predecessor of the given basic block.
       *
       * The authoritative predecessor dictates the signature difference (XOR between two block
       * signatures) used in computing the updated signature upon entering a basic block and is
       * used during instrumentation.
       */
      llvm::BasicBlock* getAuthoritativePredecessor(llvm::BasicBlock * const BB);

      /**
       * Get the authoritative sibling of the given basic block.
       *
       * In the case of fanin nodes, siblings of the fanin node's authoritative predecessor have to
       * set the runtime adjusting signature D to the signature difference between them and the
       * authoritative predecessor, i.e. their authoritative sibling. This is used in
       * instrumentation.
       */
      llvm::BasicBlock* getAuthoritativeSibling(llvm::BasicBlock * const BB);

      /**
       * Give the tail of a split block the same authoritative predecessor as the head.
       *
       * FIXME(hermannloose): This is clumsy and I'm not even sure it's needed.
       */
      void notifyAboutSplitBlock(llvm::BasicBlock * const head, llvm::BasicBlock * const tail);

    private:
      BlockToSignatureMap blockSignatures;

      BlockToBlockMap primaryPredecessors;
      BlockToBlockMap primarySiblings;

      BlockSet faninBlocks;
      // TODO(hermannloose): Rename this, since it's misleading.
      BlockSet faninSuccessors;
      unsigned long nextID;
  };

}

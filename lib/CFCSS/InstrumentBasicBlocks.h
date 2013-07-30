/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "AssignBlockSignatures.h"
#include "ReturnBlocks.h"
#include "SplitAfterCall.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {
  // FIXME(hermannloose): Remove duplication & is 64 sensible?
  typedef SmallPtrSet<BasicBlock*, 64> BlockSet;


  /**
   * Instrument all basic blocks in a module with signature checks.
   *
   * This builds upon the preprocessing and analysis performed in ReturnBlocks,
   * SplitAfterCall etc. which are scheduled in getAnalysisUsage().
   */
  class InstrumentBasicBlocks : public ModulePass {

    public:
      static char ID;

      InstrumentBasicBlocks();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module &M);

    private:
      AssignBlockSignatures *ABS;
      ReturnBlocks *RB;
      SplitAfterCall *SAC;

      BlockSet ignoreBlocks;

      void instrumentEntryBlock(BasicBlock &entryBlock);
      BasicBlock* createErrorHandlingBlock(Function *F);

      Instruction* insertSignatureUpdate(
          BasicBlock *BB,
          BasicBlock *errorHandlingBlock,
          Value *GSR,
          Value *D,
          ConstantInt *signature,
          ConstantInt *predecessorSignature,
          bool adjustForFanin,
          IRBuilder<> *builder);

      Instruction* insertRuntimeAdjustingSignature(BasicBlock &BB, Value *D, IRBuilder<> *builder);

      GlobalVariable *interFunctionGSR;
      GlobalVariable *interFunctionD;
  };

}

#pragma once

#include "Common.h"
#include "AssignBlockSignatures.h"
#include "GatewayFunctions.h"
#include "InstructionIndex.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace cfcss {

  /**
   * Instrument all basic blocks in a module with signature checks.
   *
   * This builds upon the preprocessing and analysis performed in ReturnBlocks, SplitAfterCall etc.
   * which are scheduled in getAnalysisUsage().
   */
  class InstrumentBasicBlocks : public llvm::ModulePass {

    public:
      static char ID;

      InstrumentBasicBlocks();

      virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
      virtual bool runOnModule(llvm::Module &M);

    private:
      AssignBlockSignatures *ABS;
      GatewayFunctions *GF;
      InstructionIndex *II;

      BlockSet ignoreBlocks;

      llvm::BasicBlock* createErrorHandlingBlock(llvm::Function *F);

      llvm::BasicBlock* insertSignatureUpdate(
          llvm::BasicBlock *BB,
          llvm::BasicBlock *errorHandlingBlock,
          llvm::Value *GSR,
          llvm::Value *D,
          Signature *signature,
          Signature *predecessorSignature,
          bool adjustForFanin,
          llvm::IRBuilder<> *builder);

      llvm::Instruction* insertRuntimeAdjustingSignature(
          llvm::BasicBlock &BB,
          llvm::Value *D,
          llvm::IRBuilder<> *builder);

      llvm::GlobalVariable *interFunctionGSR;
      llvm::GlobalVariable *interFunctionD;
  };

}

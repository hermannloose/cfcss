/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "AssignBlockSignatures.h"
#include "InstrumentBasicBlocks.h"

#include "llvm/ADT/APInt.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Instructions.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <stdio.h>

using namespace llvm;

namespace cfcss {

  InstrumentBasicBlocks::InstrumentBasicBlocks() : FunctionPass(ID) {}

  void InstrumentBasicBlocks::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AssignBlockSignatures>();
  }

  bool InstrumentBasicBlocks::runOnFunction(Function &F) {
    IntegerType *intType = Type::getInt64Ty(getGlobalContext());

    GlobalVariable *GSR = new GlobalVariable(
        *F.getParent(),
        intType,
        false,
        GlobalValue::PrivateLinkage,
        ConstantInt::get(intType, 0),
        Twine("GSR"));

    GlobalVariable *D = new GlobalVariable(
        *F.getParent(),
        IntegerType::get(getGlobalContext(), 64),
        false,
        GlobalValue::PrivateLinkage,
        ConstantInt::get(intType, 0),
        Twine("D"));

    AssignBlockSignatures &ABS = getAnalysis<AssignBlockSignatures>();

    for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
      BasicBlock &BB = *i;

      ConstantInt* signature = ABS.getSignature(&BB);
      bool isFanin = ABS.isFaninNode(&BB);

      BinaryOperator *signatureUpdate = NULL;
      StoreInst *storeGSR = NULL;
      BinaryOperator *correctForFanin = NULL;

      for (pred_iterator pred_i = pred_begin(&BB), pred_e = pred_end(&BB);
          pred_i != pred_e; ++pred_i) {

        BasicBlock * const predecessor = *pred_i;
        ConstantInt* pred_signature = ABS.getSignature(predecessor);

        if (pred_i == pred_begin(&BB)) {
          // We instrument the current block with instructions to update the
          // GSR and compare it against the expected signature.
          ConstantInt *signatureDiff = ConstantInt::get(intType,
              APIntOps::Xor(signature->getValue(), pred_signature->getValue()).getLimitedValue());

          if (Instruction *first = BB.getFirstNonPHI()) {
            LoadInst *loadGSR = new LoadInst(
                GSR,
                "GSR",
                first);

            signatureUpdate = BinaryOperator::Create(
                Instruction::Xor,
                loadGSR,
                signatureDiff,
                Twine("NEWGSR"),
                first);

            storeGSR = new StoreInst(
                signatureUpdate,
                GSR,
                first);
          }
        } else {
          // We instrument the predecessor to set the value D and the current
          // block to include this value in updating the GSR.
          ConstantInt *authoritativeSignature = ABS.getSignature(*pred_begin(&BB));
          ConstantInt *signatureDiff = ConstantInt::get(intType,
              APIntOps::Xor(authoritativeSignature->getValue(),
                  pred_signature->getValue()).getLimitedValue());

          StoreInst *storeD = new StoreInst(
              signatureDiff,
              D,
              predecessor->getTerminator());

          LoadInst *loadD = new LoadInst(
              D,
              "D",
              storeGSR);

          correctForFanin = BinaryOperator::Create(
              Instruction::Xor,
              signatureUpdate,
              loadD,
              Twine("FANINCORRECTED"),
              storeGSR);

          StoreInst *storeCorrected = new StoreInst(correctForFanin, GSR);
          storeCorrected->insertAfter(storeGSR);
        }
      }
    }

    return true;
  }

  char InstrumentBasicBlocks::ID = 0;
}

static RegisterPass<cfcss::InstrumentBasicBlocks>
    X("instrument-blocks", "Instrument basic blocks with CFCSS signatures", false, false);

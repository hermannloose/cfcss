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
#include "llvm/InlineAsm.h"
#include "llvm/Instructions.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <stdio.h>

using namespace llvm;

namespace cfcss {
  typedef std::pair<BasicBlock*, ConstantInt*> SignatureEntry;

  InstrumentBasicBlocks::InstrumentBasicBlocks() : FunctionPass(ID),
      splitBlocks() {}

  void InstrumentBasicBlocks::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AssignBlockSignatures>();
  }

  bool InstrumentBasicBlocks::runOnFunction(Function &F) {
    IntegerType *intType = Type::getInt64Ty(getGlobalContext());

    Instruction *insertAlloca = F.getEntryBlock().getFirstNonPHI();
    AllocaInst *GSR = new AllocaInst(intType, "GSR", insertAlloca);
    AllocaInst *D = new AllocaInst(intType, "D", insertAlloca);
    new StoreInst(ConstantInt::get(intType, 0), GSR, insertAlloca);
    new StoreInst(ConstantInt::get(intType, 0), D, insertAlloca);

    BasicBlock *errorHandlingBlock = BasicBlock::Create(
        getGlobalContext(),
        "handleSignatureFault",
        &F);

    InlineAsm *ud2 = InlineAsm::get(FunctionType::get(
        Type::getVoidTy(getGlobalContext()), ArrayRef<Type*>(), false),
        StringRef("ud2"), StringRef(), true);

    CallInst::Create(ud2, ArrayRef<Value*>(), "", errorHandlingBlock);
    new UnreachableInst(getGlobalContext(), errorHandlingBlock);

    splitBlocks.insert(SignatureEntry(errorHandlingBlock,
        ConstantInt::get(intType, 0)));

    AssignBlockSignatures &ABS = getAnalysis<AssignBlockSignatures>();

    for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
      BasicBlock &BB = *i;

      ConstantInt* signature = getSignature(&BB, ABS);

      BinaryOperator *signatureUpdate = NULL;
      StoreInst *storeGSR = NULL;
      BinaryOperator *correctForFanin = NULL;

      for (pred_iterator pred_i = pred_begin(&BB), pred_e = pred_end(&BB);
          pred_i != pred_e; ++pred_i) {

        BasicBlock * const predecessor = *pred_i;

        // FIXME(hermannloose): Verify that these are later merged with other
        // blocks or otherwise handled nicely.
        if (splitBlocks.lookup(&BB)) { continue; }

        ConstantInt* pred_signature = getSignature(predecessor, ABS);

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
                Twine("GSR"),
                first);

            storeGSR = new StoreInst(
                signatureUpdate,
                GSR,
                first);
          }
        } else {
          // We instrument the predecessor to set the value D and the current
          // block to include this value in updating the GSR.
          ConstantInt *authoritativeSignature = getSignature(*pred_begin(&BB), ABS);
          ConstantInt *signatureDiff = ConstantInt::get(intType,
              APIntOps::Xor(authoritativeSignature->getValue(),
                  pred_signature->getValue()).getLimitedValue());

          new StoreInst(
              ConstantInt::get(intType, 0),
              D,
              (*pred_begin(&BB))->getTerminator());

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
              Twine("GSR"),
              storeGSR);

          StoreInst *storeCorrected = new StoreInst(correctForFanin, GSR);
          storeCorrected->insertAfter(storeGSR);
          storeGSR = storeCorrected;
        }
      }

      // FIXME(hermannloose): False for the entry block. Handle cleaner.
      if (storeGSR) {
        LoadInst *loadGSR = new LoadInst(GSR, "GSR");
        loadGSR->insertAfter(storeGSR);

        ICmpInst *compareSignatures = new ICmpInst(
            CmpInst::ICMP_EQ,
            loadGSR,
            signature,
            "SIGEQ");

        compareSignatures->insertAfter(loadGSR);

        BasicBlock::iterator nextInst(compareSignatures);
        ++nextInst;
        BasicBlock *oldTerminatorBlock = SplitBlock(&BB, nextInst, this);
        splitBlocks.insert(SignatureEntry(oldTerminatorBlock, signature));
        BB.getTerminator()->eraseFromParent();

        BranchInst *errorHandling = BranchInst::Create(
            oldTerminatorBlock,
            errorHandlingBlock,
            compareSignatures,
            &BB);
      }

      DEBUG(BB.dump());
    }

    DEBUG(errs() << "Run on function " << F.getName().str() << " complete.\n");

    return true;
  }

  ConstantInt* InstrumentBasicBlocks::getSignature(BasicBlock * const BB,
      AssignBlockSignatures &ABS) {

    ConstantInt *signature = ABS.getSignature(BB);
    if (!signature) {
      signature = splitBlocks.lookup(BB);
    }

    assert(signature && "We should have a valid signature at this point!");

    return signature;
  }

  char InstrumentBasicBlocks::ID = 0;
}

static RegisterPass<cfcss::InstrumentBasicBlocks>
    X("instrument-blocks", "Instrument basic blocks with CFCSS signatures", false, false);

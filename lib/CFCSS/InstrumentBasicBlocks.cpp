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
      ignoreBlocks() {}

  bool InstrumentBasicBlocks::doInitialization(Module &M) {
    interFunctionGSR = new GlobalVariable(
        M,
        Type::getInt64Ty(getGlobalContext()),
        false, /* isConstant */
        GlobalValue::PrivateLinkage,
        ConstantInt::get(Type::getInt64Ty(getGlobalContext()), 0),
        "interFunctionGSR");

    return false;
  }

  void InstrumentBasicBlocks::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AssignBlockSignatures>();
    AU.addPreserved<AssignBlockSignatures>();
  }

  bool InstrumentBasicBlocks::runOnFunction(Function &F) {
    IntegerType *intType = Type::getInt64Ty(getGlobalContext());
    AssignBlockSignatures &ABS = getAnalysis<AssignBlockSignatures>();

    instrumentEntryBlock(F.getEntryBlock());
    BasicBlock *errorHandlingBlock = createErrorHandlingBlock(F);

    for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
      if (ignoreBlocks.lookup(i)) {
        // Ignore the remainders of previously split blocks, as well as the
        // entry block, which is handled separately.
        continue;
      }

      BasicBlock &BB = *i;
      BasicBlock *authPred = ABS.getAuthoritativePredecessor(&BB);

      ConstantInt* signature = ABS.getSignature(&BB);
      ConstantInt* pred_signature = ABS.getSignature(authPred);
      ConstantInt *signatureDiff = ConstantInt::get(intType,
          APIntOps::Xor(signature->getValue(), pred_signature->getValue()).getLimitedValue());

      if (Instruction *first = BB.getFirstNonPHI()) {
        LoadInst *loadGSR = new LoadInst(
            GSR,
            "GSR",
            first);

        BinaryOperator *signatureUpdate = BinaryOperator::Create(
            Instruction::Xor,
            loadGSR,
            signatureDiff,
            Twine("GSR"),
            first);

        if (ABS.isFaninNode(&BB)) {
          LoadInst *loadD = new LoadInst(
              D,
              "D",
              first);

          signatureUpdate = BinaryOperator::Create(
              Instruction::Xor,
              signatureUpdate,
              loadD,
              Twine("GSR"),
              first);
        }

        StoreInst *storeGSR = new StoreInst(
            signatureUpdate,
            GSR,
            first);

        ICmpInst *compareSignatures = new ICmpInst(
            first,
            CmpInst::ICMP_EQ,
            signatureUpdate,
            signature,
            "SIGEQ");


        // We branch after the comparison, so we split the block there.
        BasicBlock::iterator nextInst(compareSignatures);
        ++nextInst;

        BasicBlock *oldTerminatorBlock = SplitBlock(&BB, nextInst, this);
        ignoreBlocks.insert(SignatureEntry(oldTerminatorBlock, signature));
        BB.getTerminator()->eraseFromParent();

        BranchInst *errorHandling = BranchInst::Create(
            oldTerminatorBlock,
            errorHandlingBlock,
            compareSignatures,
            &BB);
      }

      // Store signature adjustment in register D if there is a successor that
      // is a fanin node.
      if (ABS.hasFaninSuccessor(&BB)) {
        // If this is actually our block, we do want to store 0 in D, so
        // there's no special treatment here.
        BasicBlock *authSibling = ABS.getAuthoritativeSibling(&BB);

        ConstantInt* siblingSignature = ABS.getSignature(authSibling);
        ConstantInt *signatureAdjustment = ConstantInt::get(intType,
          APIntOps::Xor(signature->getValue(), siblingSignature->getValue()).getLimitedValue());

        new StoreInst(signatureAdjustment, D, BB.getTerminator());
      }

      DEBUG(BB.dump());
    }

    DEBUG(errs() << "Run on function " << F.getName().str() << " complete.\n");

    return true;
  }


  void InstrumentBasicBlocks::instrumentEntryBlock(BasicBlock &entryBlock) {
    IntegerType *intType = Type::getInt64Ty(getGlobalContext());
    Instruction *insertAlloca = entryBlock.getFirstNonPHI();

    GSR = new AllocaInst(intType, "GSR", insertAlloca);
    D = new AllocaInst(intType, "D", insertAlloca);

    LoadInst *loadInterFunctionGSR = new LoadInst(interFunctionGSR, "GSR", insertAlloca);
    new StoreInst(loadInterFunctionGSR, GSR, insertAlloca);
    new StoreInst(ConstantInt::get(intType, 0), D, insertAlloca);

    // FIXME(hermannloose): Adapt instrumentation with checking code.
    ignoreBlocks.insert(SignatureEntry(&entryBlock, ConstantInt::get(intType, 0)));
  }


  // FIXME(hermannloose): How to access ABS?
  void InstrumentBasicBlocks::instrumentBlock(BasicBlock &BB, Instruction *insertBefore) {
    // TODO(hermannloose): Pull lots of code from runOnFunction() in here.
  }


  BasicBlock* InstrumentBasicBlocks::createErrorHandlingBlock(Function &F) {
    IntegerType *intType = Type::getInt64Ty(getGlobalContext());
    BasicBlock *errorHandlingBlock = BasicBlock::Create(
        getGlobalContext(),
        "handleSignatureFault",
        &F);

    InlineAsm *ud2 = InlineAsm::get(FunctionType::get(
        Type::getVoidTy(getGlobalContext()), ArrayRef<Type*>(), false),
        StringRef("ud2"), StringRef(), true);

    CallInst::Create(ud2, ArrayRef<Value*>(), "", errorHandlingBlock);
    new UnreachableInst(getGlobalContext(), errorHandlingBlock);

    ignoreBlocks.insert(SignatureEntry(errorHandlingBlock,
        ConstantInt::get(intType, 0)));

    return errorHandlingBlock;
  }


  /**
   *
   */
  ConstantInt* InstrumentBasicBlocks::getSignature(BasicBlock * const BB,
      AssignBlockSignatures &ABS) {

    ConstantInt *signature = ABS.getSignature(BB);
    if (!signature) {
      signature = ignoreBlocks.lookup(BB);
    }

    assert(signature && "We should have a valid signature at this point!");

    return signature;
  }

  char InstrumentBasicBlocks::ID = 0;
}

static RegisterPass<cfcss::InstrumentBasicBlocks>
    X("instrument-blocks", "Instrument basic blocks with CFCSS signatures", false, false);

/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "InstrumentBasicBlocks.h"

#include "AssignBlockSignatures.h"
#include "RemoveCFGAliasing.h"
#include "SplitAfterCall.h"

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

static const char *debugPrefix = "InstrumentBasicBlocks: ";

namespace cfcss {
  InstrumentBasicBlocks::InstrumentBasicBlocks() : ModulePass(ID),
      ignoreBlocks() {}

  void InstrumentBasicBlocks::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredTransitive<AssignBlockSignatures>();
    AU.addRequiredTransitive<SplitAfterCall>();

    AU.addPreserved<AssignBlockSignatures>();
    AU.addPreserved<RemoveCFGAliasing>();
    AU.addPreserved<SplitAfterCall>();
  }

  bool InstrumentBasicBlocks::runOnModule(Module &M) {
    SplitAfterCall &SAC = getAnalysis<SplitAfterCall>();

    interFunctionGSR = new GlobalVariable(
        M,
        Type::getInt64Ty(getGlobalContext()),
        false, /* isConstant */
        GlobalValue::PrivateLinkage,
        ConstantInt::get(Type::getInt64Ty(getGlobalContext()), 0),
        "interFunctionGSR");

    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      if (fi->isDeclaration()) {
        DEBUG(errs() << debugPrefix << "Skipping [" << fi->getName() << "], is a declaration.\n");
        continue;
      }

      DEBUG(errs() << debugPrefix << "Running on [" << fi->getName() << "].\n");

      instrumentEntryBlock(fi->getEntryBlock());
      BasicBlock *errorHandlingBlock = createErrorHandlingBlock(fi);

      for (Function::iterator i = fi->begin(), e = fi->end(); i != e; ++i) {
        if (ignoreBlocks.count(i)) {
          DEBUG(errs() << debugPrefix << "Ignoring [" << i->getName() << "].\n");
          // Ignore the remainders of previously split blocks, as well as the
          // entry block, which is handled separately.
          continue;
        }

        BasicBlock &BB = *i;
        Instruction *insertionPoint = BB.getFirstNonPHI();

        instrumentBlock(BB, errorHandlingBlock, insertionPoint);

        if (ABS->hasFaninSuccessor(&BB)) {
          DEBUG(errs() << debugPrefix << "[" << BB.getName()
              << "] has a fanin successors, setting D.\n");

          insertRuntimeAdjustingSignature(BB);
        }

        DEBUG(BB.dump());
      }

      DEBUG(errs() << debugPrefix << "Run on function " << fi->getName().str() << " complete.\n");
    }

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
    ignoreBlocks.insert(&entryBlock);
  }


  Instruction* InstrumentBasicBlocks::instrumentBlock(BasicBlock &BB, BasicBlock *errorHandlingBlock,
      Instruction *insertBefore) {

    DEBUG(errs() << debugPrefix << "Instrumenting [" << BB.getName() << "]\n");

    BasicBlock *authPred = ABS->getAuthoritativePredecessor(&BB);
    assert(authPred);

    // Compute the signature update.
    ConstantInt* signature = ABS->getSignature(&BB);
    assert(signature);
    ConstantInt* pred_signature = ABS->getSignature(authPred);
    assert(pred_signature);
    ConstantInt *signatureDiff = ConstantInt::get(Type::getInt64Ty(getGlobalContext()),
        APIntOps::Xor(signature->getValue(), pred_signature->getValue()).getLimitedValue());

    LoadInst *loadGSR = new LoadInst(
        GSR,
        "GSR",
        insertBefore);

    BinaryOperator *signatureUpdate = BinaryOperator::Create(
        Instruction::Xor,
        loadGSR,
        signatureDiff,
        Twine("GSR"),
        insertBefore);

    if (ABS->isFaninNode(&BB)) {
      LoadInst *loadD = new LoadInst(
          D,
          "D",
          insertBefore);

      signatureUpdate = BinaryOperator::Create(
          Instruction::Xor,
          signatureUpdate,
          loadD,
          Twine("GSR"),
          insertBefore);
    }

    StoreInst *storeGSR = new StoreInst(
        signatureUpdate,
        GSR,
        insertBefore);

    ICmpInst *compareSignatures = new ICmpInst(
        insertBefore,
        CmpInst::ICMP_EQ,
        signatureUpdate,
        signature,
        "SIGEQ");


    // We branch after the comparison, so we split the block there.
    BasicBlock::iterator nextInst(compareSignatures);
    ++nextInst;

    BasicBlock *oldTerminatorBlock = SplitBlock(&BB, nextInst, this);
    ignoreBlocks.insert(oldTerminatorBlock);
    ABS->notifyAboutSplitBlock(&BB, oldTerminatorBlock);

    BB.getTerminator()->eraseFromParent();
    BranchInst *errorHandling = BranchInst::Create(
        oldTerminatorBlock,
        errorHandlingBlock,
        compareSignatures,
        &BB);

    return errorHandling;
  }


  Instruction* InstrumentBasicBlocks::insertRuntimeAdjustingSignature(BasicBlock &BB) {
    // If this is actually our block, we do want to store 0 in D, so
    // there's no special treatment here.
    BasicBlock *authSibling = ABS->getAuthoritativeSibling(&BB);

    ConstantInt* signature = ABS->getSignature(&BB);
    ConstantInt* siblingSignature = ABS->getSignature(authSibling);
    ConstantInt *signatureAdjustment = ConstantInt::get(Type::getInt64Ty(getGlobalContext()),
      APIntOps::Xor(signature->getValue(), siblingSignature->getValue()).getLimitedValue());

    return new StoreInst(signatureAdjustment, D, BB.getTerminator());
  }


  BasicBlock* InstrumentBasicBlocks::createErrorHandlingBlock(Function *F) {
    BasicBlock *errorHandlingBlock = BasicBlock::Create(
        getGlobalContext(),
        "handleSignatureFault",
        F);

    InlineAsm *ud2 = InlineAsm::get(FunctionType::get(
        Type::getVoidTy(getGlobalContext()), ArrayRef<Type*>(), false),
        StringRef("ud2"), StringRef(), true);

    CallInst::Create(ud2, ArrayRef<Value*>(), "", errorHandlingBlock);
    new UnreachableInst(getGlobalContext(), errorHandlingBlock);

    ignoreBlocks.insert(errorHandlingBlock);

    DEBUG(errs() << debugPrefix << "Created error handling block.\n");
    DEBUG(errorHandlingBlock->dump());

    return errorHandlingBlock;
  }


  char InstrumentBasicBlocks::ID = 0;
}

static RegisterPass<cfcss::InstrumentBasicBlocks>
    X("instrument-blocks", "Instrument basic blocks (CFCSS)", false, false);

/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "InstrumentBasicBlocks.h"

#include "AssignBlockSignatures.h"
#include "GatewayFunctions.h"
#include "RemoveCFGAliasing.h"
#include "SplitAfterCall.h"

#include "llvm/ADT/APInt.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
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
    AU.addRequiredTransitive<GatewayFunctions>();
    AU.addRequiredTransitive<ReturnBlocks>();
    AU.addRequiredTransitive<SplitAfterCall>();

    // TODO(hermannloose): AU.setPreservesAll() would probably not hurt.
    AU.addPreserved<AssignBlockSignatures>();
    AU.addPreserved<GatewayFunctions>();
    AU.addPreserved<RemoveCFGAliasing>();
    AU.addPreserved<ReturnBlocks>();
    AU.addPreserved<SplitAfterCall>();
  }


  bool InstrumentBasicBlocks::runOnModule(Module &M) {
    ABS = &getAnalysis<AssignBlockSignatures>();
    RB = &getAnalysis<ReturnBlocks>();
    SAC = &getAnalysis<SplitAfterCall>();

    interFunctionGSR = new GlobalVariable(
        M,
        Type::getInt64Ty(getGlobalContext()),
        false, /* isConstant */
        GlobalValue::LinkOnceAnyLinkage,
        ConstantInt::get(Type::getInt64Ty(getGlobalContext()), 0),
        "interFunctionGSR");

    interFunctionD = new GlobalVariable(
        M,
        Type::getInt64Ty(getGlobalContext()),
        false, /* isConstant */
        GlobalValue::LinkOnceAnyLinkage,
        ConstantInt::get(Type::getInt64Ty(getGlobalContext()), 0),
        "interFunctionD");

    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      if (fi->isDeclaration()) {
        DEBUG(errs() << debugPrefix << "Skipping [" << fi->getName() << "] (declaration)\n");
        continue;
      }

      if (fi->isIntrinsic()) {
        DEBUG(errs() << debugPrefix << "Skipping [" << fi->getName() << "] (intrinsic)\n");

        continue;
      }

      DEBUG(errs() << debugPrefix << "Running on [" << fi->getName() << "].\n");
      IntegerType *intType = Type::getInt64Ty(getGlobalContext());
      BasicBlock &entryBlock = fi->getEntryBlock();
      IRBuilder<> builder(entryBlock.getFirstNonPHI());

      Value *GSR = builder.CreateAlloca(intType, 0, "GSR");
      Value *D = builder.CreateAlloca(intType, 0, "D");

      BasicBlock *errorHandlingBlock = createErrorHandlingBlock(fi);


      instrumentEntryBlock(fi->getEntryBlock());

      for (Function::iterator i = fi->begin(), e = fi->end(); i != e; ++i) {
        if (ignoreBlocks.count(i)) {
          DEBUG(errs() << debugPrefix << "Ignoring [" << i->getName() << "].\n");
          // Ignore the remainders of previously split blocks, as well as the
          // entry block, which is handled separately.
          continue;
        }

        builder.SetInsertPoint(i->getFirstNonPHI());

        if (SAC->wasSplitAfterCall(i)) {
          builder.CreateStore(builder.CreateLoad(interFunctionGSR, "GSR"), GSR);
          builder.CreateStore(builder.CreateLoad(interFunctionD, "D"), D);

          Function *calledFunction = SAC->getCalledFunctionForReturnBlock(i);
          DEBUG(errs() << debugPrefix << "[" << i->getName() << "] was split after call to ["
              << calledFunction->getName() << "].\n");

          BasicBlock *authoritativeReturnBlock = RB->getAuthoritativeReturnBlock(calledFunction);
          assert(authoritativeReturnBlock);
          BlockSet *returnBlocks = RB->getReturnBlocks(calledFunction);
          assert(authoritativeReturnBlock);

          insertSignatureUpdate(
              i,
              errorHandlingBlock,
              GSR,
              D,
              ABS->getSignature(i),
              ABS->getSignature(authoritativeReturnBlock),
              (returnBlocks->size() > 1), /* adjustForFanin */
              &builder);

        } else {
          BasicBlock *authoritativePredecessor = ABS->getAuthoritativePredecessor(i);
          assert(authoritativePredecessor);

          insertSignatureUpdate(
              i,
              errorHandlingBlock,
              GSR,
              D,
              ABS->getSignature(i),
              ABS->getSignature(authoritativePredecessor),
              ABS->isFaninNode(i), /* adjustForFanin */
              &builder);
        }

        // TODO(hermannloose): Put this towards the end of block processing.
        if (isa<ReturnInst>(i->getTerminator())) {
          // TODO(hermannloose): Might need to have D set if more than one.
          DEBUG(errs() << debugPrefix << "[" << i->getName() << "] is a return block.");
        }

        if (ABS->hasFaninSuccessor(i)) {
          DEBUG(errs() << debugPrefix << "[" << i->getName()
              << "] has a fanin successors, setting D.\n");

          builder.SetInsertPoint(i->getTerminator());

          insertRuntimeAdjustingSignature(*i, D, &builder);
        }
      }

      DEBUG(errs() << debugPrefix << "Run on function " << fi->getName().str() << " complete.\n");
    }

    return true;
  }


  void InstrumentBasicBlocks::instrumentEntryBlock(BasicBlock &entryBlock) {
    IntegerType *intType = Type::getInt64Ty(getGlobalContext());
    Instruction *insertAlloca = entryBlock.getFirstNonPHI();

    LoadInst *loadInterFunctionGSR = new LoadInst(interFunctionGSR, "GSR", insertAlloca);
    LoadInst *loadInterFunctionD = new LoadInst(interFunctionD, "D", insertAlloca);

    // FIXME(hermannloose): Adapt instrumentation with checking code.
    ignoreBlocks.insert(&entryBlock);
  }


  Instruction* InstrumentBasicBlocks::insertSignatureUpdate(
      BasicBlock *BB,
      BasicBlock *errorHandlingBlock,
      Value *GSR,
      Value *D,
      ConstantInt *signature,
      ConstantInt *predecessorSignature,
      bool adjustForFanin,
      IRBuilder<> *builder) {

    assert(BB);
    assert(errorHandlingBlock);
    assert(signature);
    assert(predecessorSignature);
    assert(builder);

    DEBUG(errs() << debugPrefix << "Instrumenting [" << BB->getName() << "]\n");

    // Compute the signature update.
    ConstantInt *signatureDiff = ConstantInt::get(builder->getInt64Ty(),
        APIntOps::Xor(signature->getValue(), predecessorSignature->getValue()).getLimitedValue());

    LoadInst *loadGSR = builder->CreateLoad(GSR, "GSR");
    Value *signatureUpdate = builder->CreateXor(loadGSR, signatureDiff, "GSR");

    if (adjustForFanin) {
      LoadInst *loadD = builder->CreateLoad(D, "D");
      signatureUpdate = builder->CreateXor(signatureUpdate, loadD, "GSR");
    }

    builder->CreateStore(signatureUpdate, GSR);
    Value *compareSignatures = builder->CreateICmpEQ(signatureUpdate, signature, "SIGEQ");

    // We branch after the comparison, so we split the block there.
    BasicBlock *oldTerminatorBlock = SplitBlock(BB, builder->GetInsertPoint(), this);
    ignoreBlocks.insert(oldTerminatorBlock);
    // TODO(hermannloose): Remove dependency on ABS.
    ABS->notifyAboutSplitBlock(BB, oldTerminatorBlock);

    BB->getTerminator()->eraseFromParent();
    builder->SetInsertPoint(BB);

    BranchInst *errorHandling =
      builder->CreateCondBr(compareSignatures, oldTerminatorBlock, errorHandlingBlock);

    return errorHandling;
  }


  Instruction* InstrumentBasicBlocks::insertRuntimeAdjustingSignature(BasicBlock &BB, Value *D,
      IRBuilder<> *builder) {
    // If this is actually our block, we do want to store 0 in D, so
    // there's no special treatment here.
    BasicBlock *authSibling = ABS->getAuthoritativeSibling(&BB);

    ConstantInt* signature = ABS->getSignature(&BB);
    ConstantInt* siblingSignature = ABS->getSignature(authSibling);
    ConstantInt *signatureAdjustment = ConstantInt::get(builder->getInt64Ty(),
        APIntOps::Xor(signature->getValue(), siblingSignature->getValue()).getLimitedValue());

    return builder->CreateStore(signatureAdjustment, D);
  }


  BasicBlock* InstrumentBasicBlocks::createErrorHandlingBlock(Function *F) {
    BasicBlock *errorHandlingBlock = BasicBlock::Create(
        getGlobalContext(),
        "handleSignatureFault",
        F);

    IRBuilder<> builder(errorHandlingBlock);

    InlineAsm *ud2 =
        InlineAsm::get(FunctionType::get(builder.getVoidTy(), ArrayRef<Type*>(), false),
            StringRef("ud2"), StringRef(), true);

    builder.CreateCall(ud2);
    builder.CreateUnreachable();

    ignoreBlocks.insert(errorHandlingBlock);

    DEBUG(errs() << debugPrefix << "Created error handling block.\n");

    return errorHandlingBlock;
  }


  char InstrumentBasicBlocks::ID = 0;
}

static RegisterPass<cfcss::InstrumentBasicBlocks>
    X("instrument-blocks", "Instrument Basic Blocks (CFCSS)");

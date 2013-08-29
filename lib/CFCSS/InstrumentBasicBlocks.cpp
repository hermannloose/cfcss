#define DEBUG_TYPE "cfcss-instrument-blocks"

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
    AU.addRequiredTransitive<GatewayFunctions>();
    AU.addRequiredTransitive<SplitAfterCall>();
    AU.addRequiredTransitive<RemoveCFGAliasing>();
    AU.addRequiredTransitive<AssignBlockSignatures>();
    AU.addRequiredTransitive<InstructionIndex>();

    // TODO(hermannloose): AU.setPreservesAll() would probably not hurt.
    AU.addPreserved<AssignBlockSignatures>();
    AU.addPreserved<GatewayFunctions>();
    AU.addPreserved<RemoveCFGAliasing>();
    AU.addPreserved<SplitAfterCall>();
    AU.addPreserved<InstructionIndex>();
  }


  bool InstrumentBasicBlocks::runOnModule(Module &M) {
    GF = &getAnalysis<GatewayFunctions>();
    II = &getAnalysis<InstructionIndex>();
    SAC = &getAnalysis<SplitAfterCall>();
    RemoveCFGAliasing *RCA = &getAnalysis<RemoveCFGAliasing>();
    ABS = &getAnalysis<AssignBlockSignatures>();

    IntegerType *intType = Type::getInt64Ty(getGlobalContext());

    interFunctionGSR = new GlobalVariable(
        M,
        intType,
        false, /* isConstant */
        GlobalValue::LinkOnceAnyLinkage,
        ConstantInt::get(intType, 0),
        "interFunctionGSR");

    interFunctionD = new GlobalVariable(
        M,
        intType,
        false, /* isConstant */
        GlobalValue::LinkOnceAnyLinkage,
        ConstantInt::get(intType, 0),
        "interFunctionD");

    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      if (fi->isDeclaration()) {
        DEBUG(errs() << debugPrefix << "Skipping [" << fi->getName() << "] (declaration)\n");
        continue;
      }

      // TODO(hermannloose): Check whether these can occur at all.
      if (fi->isIntrinsic()) {
        DEBUG(errs() << debugPrefix << "Skipping [" << fi->getName() << "] (intrinsic)\n");

        continue;
      }

      DEBUG(errs() << debugPrefix << "Running on [" << fi->getName() << "].\n");

      // Initialize CFCSS "registers", i.e. local variables.

      BasicBlock *entryBlock = &fi->getEntryBlock();
      IRBuilder<> builder(entryBlock->getFirstNonPHI());

      Value *GSR = builder.CreateAlloca(intType, 0, "GSR");
      Value *D = builder.CreateAlloca(intType, 0, "D");

      builder.CreateStore(ConstantInt::get(intType, 0), GSR);
      builder.CreateStore(ConstantInt::get(intType, 0), D);

      BasicBlock *errorHandlingBlock = createErrorHandlingBlock(fi);

      DEBUG(errs() << debugPrefix << "Instrumenting entry block.\n");

      // TODO(hermannloose): Maybe delegate to two functions for this.
      if (GF->isGateway(fi)) {
        Signature *signature = ABS->getSignature(entryBlock);
        builder.CreateStore(signature, GSR);

        Function *internal = GF->getInternalFunction(fi);
        if (GF->isFaninNode(internal)) {
          // TODO(hermannloose): Duplication below, factor out.
          Function *authoritativePredecessor = GF->getAuthoritativePredecessor(internal);
          CallInst *callSite = II->getPrimaryCallTo(internal, authoritativePredecessor);
          assert(callSite && "Function should have an authoritative call site!");
          BasicBlock *callingBlock = callSite->getParent();
          assert(callingBlock && "Call site should be part of a basic block!");
          Signature *predecessorSignature = ABS->getSignature(callingBlock);

          ConstantInt *signatureAdjustment = ConstantInt::get(getGlobalContext(),
              APIntOps::Xor(signature->getValue(), predecessorSignature->getValue()));

          builder.CreateStore(signatureAdjustment, D);
        }
      } else {
        Function *authoritativePredecessor = GF->getAuthoritativePredecessor(fi);
        assert(authoritativePredecessor && "Function should have an authoritative predecessor!");
        CallInst *callSite = II->getPrimaryCallTo(fi, authoritativePredecessor);
        assert(callSite && "Function should have an authoritative call site!");
        BasicBlock *callingBlock = callSite->getParent();
        assert(callingBlock && "Call site should be part of a basic block!");

        ConstantInt *predecessorSignature = ABS->getSignature(callingBlock);
        assert(predecessorSignature && "Calling basic block should have a signature!");

        builder.CreateStore(builder.CreateLoad(interFunctionGSR, "GSR"), GSR);
        builder.CreateStore(builder.CreateLoad(interFunctionD, "D"), D);

        BasicBlock *entryBlockRemainder = insertSignatureUpdate(
            entryBlock,
            errorHandlingBlock,
            GSR,
            D,
            ABS->getSignature(entryBlock),
            predecessorSignature,
            GF->isFaninNode(fi), /* adjustForFanin */
            &builder);

        // TODO(hermannloose): Move somewhere else, this stuff is all over the
        // place.
        if (ABS->hasFaninSuccessor(entryBlock)) {
          builder.SetInsertPoint(entryBlockRemainder->getTerminator());
          insertRuntimeAdjustingSignature(*entryBlockRemainder, D, &builder);
        }
      }

      ignoreBlocks.insert(entryBlock);

      DEBUG(errs() << debugPrefix << "Instrumenting basic blocks.\n");

      for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
        if (ignoreBlocks.count(bi)) {
          // Ignore the remainders of previously split blocks.
          continue;
        }

        builder.SetInsertPoint(bi->getFirstNonPHI());

        BasicBlock *remainder = NULL;

        if (SAC->wasSplitAfterCall(bi)) {
          // Check for a valid control flow transfer from one of the return
          // blocks of the function that was called from the basic block
          // preceding the current one.
          builder.CreateStore(builder.CreateLoad(interFunctionGSR, "GSR"), GSR);
          builder.CreateStore(builder.CreateLoad(interFunctionD, "D"), D);

          Function *calledFunction = SAC->getCalledFunctionForReturnBlock(bi);

          ReturnInst *primaryReturn = II->getPrimaryReturn(calledFunction);
          BasicBlock *authoritativeReturnBlock = primaryReturn->getParent();
          assert(authoritativeReturnBlock);
          ReturnList *returns = II->getReturns(calledFunction);
          assert(returns);

          remainder = insertSignatureUpdate(
              bi,
              errorHandlingBlock,
              GSR,
              D,
              ABS->getSignature(bi),
              ABS->getSignature(authoritativeReturnBlock),
              (returns->size() > 1), /* adjustForFanin */
              &builder);

        } else {
          // Check for a valid control flow transfer from one of the
          // predecessors of the current basic block.
          BasicBlock *authoritativePredecessor = ABS->getAuthoritativePredecessor(bi);
          assert(authoritativePredecessor);

          remainder = insertSignatureUpdate(
              bi,
              errorHandlingBlock,
              GSR,
              D,
              ABS->getSignature(bi),
              ABS->getSignature(authoritativePredecessor),
              ABS->isFaninNode(bi), /* adjustForFanin */
              &builder);
        }

        builder.SetInsertPoint(remainder->getTerminator());

        if (ABS->hasFaninSuccessor(bi)) {
          insertRuntimeAdjustingSignature(*remainder, D, &builder);
        }
      }

      DEBUG(errs() << debugPrefix << "Instrumenting call sites.\n");

      CallList *calls = II->getCalls(fi);
      for (CallList::iterator ci = calls->begin(), ce = calls->end(); ci != ce; ++ci) {
        CallInst *callInst = *ci;
        Function *callee = callInst->getCalledFunction();

        builder.SetInsertPoint(callInst);
        builder.CreateStore(builder.CreateLoad(GSR, "GSR"), interFunctionGSR);

        if (GF->isFaninNode(callee)) {
          // Set runtime adjusting signature.
          // TODO(hermannloose): Factor out.
          Function *authoritativePredecessor = GF->getAuthoritativePredecessor(callee);
          CallInst *primaryCall = II->getPrimaryCallTo(callee, authoritativePredecessor);

          Signature *sigA = ABS->getSignature(callInst->getParent());
          Signature *sigB = ABS->getSignature(primaryCall->getParent());
          Signature *signatureAdjustment = Signature::get(getGlobalContext(),
              APIntOps::Xor(sigA->getValue(), sigB->getValue()));

          builder.CreateStore(signatureAdjustment, interFunctionD);
        }
      }

      DEBUG(errs() << debugPrefix << "Instrumenting return blocks.\n");

      if (!II->doesNotReturn(fi)) {
        if (GF->isGateway(fi)) {
          ReturnInst *returnInst = II->getPrimaryReturn(fi);

          builder.SetInsertPoint(returnInst);

          builder.CreateStore(ConstantInt::get(intType, 0), interFunctionGSR);
          builder.CreateStore(ConstantInt::get(intType, 0), interFunctionD);
        } else {
          ReturnInst *primaryReturn = II->getPrimaryReturn(fi);
          ConstantInt *sigA = ABS->getSignature(primaryReturn->getParent());

          ReturnList *returns = II->getReturns(fi);
          for (ReturnList::iterator ri = returns->begin(), re = returns->end(); ri != re; ++ri) {
            ConstantInt *sigB = ABS->getSignature((*ri)->getParent());
            ConstantInt *signatureAdjustment = ConstantInt::get(getGlobalContext(),
                APIntOps::Xor(sigA->getValue(), sigB->getValue()));

            builder.SetInsertPoint(*ri);
            builder.CreateStore(builder.CreateLoad(GSR, "GSR"), interFunctionGSR);
            builder.CreateStore(signatureAdjustment, interFunctionD);
          }
        }
      }

      DEBUG(
        errs() << debugPrefix;
        errs().changeColor(raw_ostream::GREEN);
        errs() << "Finished on [" << fi->getName() << "].\n";
        errs().resetColor();
      );
    }

    return true;
  }


  BasicBlock* InstrumentBasicBlocks::insertSignatureUpdate(
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

    // Compute the signature update.
    ConstantInt *signatureDiff = ConstantInt::get(getGlobalContext(),
        APIntOps::Xor(signature->getValue(), predecessorSignature->getValue()));

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

    return oldTerminatorBlock;
  }


  Instruction* InstrumentBasicBlocks::insertRuntimeAdjustingSignature(BasicBlock &BB, Value *D,
      IRBuilder<> *builder) {
    // If this is actually our block, we do want to store 0 in D, so
    // there's no special treatment here.
    BasicBlock *authSibling = ABS->getAuthoritativeSibling(&BB);

    Signature* signature = ABS->getSignature(&BB);
    Signature* siblingSignature = ABS->getSignature(authSibling);
    Signature *signatureAdjustment = ConstantInt::get(getGlobalContext(),
        APIntOps::Xor(signature->getValue(), siblingSignature->getValue()));

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


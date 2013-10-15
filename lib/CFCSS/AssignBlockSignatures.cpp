#define DEBUG_TYPE "cfcss-assign-block-signatures"

#include "AssignBlockSignatures.h"

#include "llvm/ADT/APInt.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static const char *debugPrefix = "AssignBlockSignatures: ";

namespace cfcss {

  llvm::cl::opt<bool> Signatures32("cfcss-signatures-32bit",
      llvm::cl::desc("Use 32-bit signatures for CFCSS."));

  AssignBlockSignatures::AssignBlockSignatures() : ModulePass(ID),
      blockSignatures(),
      primaryPredecessors(),
      primarySiblings(),
      faninBlocks(),
      faninSuccessors(),
      nextID(0) {
  }


  void AssignBlockSignatures::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }


  bool AssignBlockSignatures::runOnModule(Module &M) {
    IntegerType *intType = NULL;
    if (Signatures32.getValue()) {
      intType = Type::getInt32Ty(getGlobalContext());
    } else {
      intType = Type::getInt64Ty(getGlobalContext());
    }

    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      if (fi->isDeclaration()) {
        DEBUG(errs() << debugPrefix << "Skipping [" << fi->getName() << "] (declaration)\n");

        continue;
      }

      DEBUG(errs() << debugPrefix << "Running on [" << fi->getName() << "] ... ");

      for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi, ++nextID) {
        blockSignatures.insert(BlockToSignatureEntry(bi, Signature::get(intType, nextID)));
        bi->setName(Twine("0x") + Twine::utohexstr(nextID) + Twine(": ") + bi->getName());

        for (succ_iterator si = succ_begin(bi), se = succ_end(bi); si != se; ++si) {
          BasicBlock *succ = *si;
          if (++pred_begin(succ) != pred_end(succ)) {
            // Successor is a fanin node.
            faninBlocks.insert(succ);
            if (!primaryPredecessors.count(succ)) {
              primaryPredecessors.insert(BlockToBlockEntry(succ, bi));
              for (pred_iterator pi = pred_begin(succ), pe = pred_end(succ); pi != pe; ++pi) {
                primarySiblings.insert(BlockToBlockEntry(*pi, bi));
                faninSuccessors.insert(*pi);
              }
            }
          } else {
            primaryPredecessors.insert(BlockToBlockEntry(succ, bi));
          }
        }
      }

      DEBUG(
        errs().changeColor(raw_ostream::GREEN);
        errs() << "done\n";
        errs().resetColor();
      );
    }

    return false;
  }


  Signature* AssignBlockSignatures::getSignature(BasicBlock * const BB) {
    return blockSignatures.lookup(BB);
  }


  BasicBlock* AssignBlockSignatures::getAuthoritativePredecessor(BasicBlock * const BB) {
    return primaryPredecessors.lookup(BB);
  }


  BasicBlock* AssignBlockSignatures::getAuthoritativeSibling(BasicBlock * const BB) {
    return primarySiblings.lookup(BB);
  }


  void AssignBlockSignatures::notifyAboutSplitBlock(BasicBlock * const head,
      BasicBlock * const tail) {

    // TODO(hermannloose): Dirty hack (or is it?)
    primarySiblings.insert(BlockToBlockEntry(tail, primarySiblings.lookup(head)));
    blockSignatures.insert(BlockToSignatureEntry(tail, blockSignatures.lookup(head)));
  }


  bool AssignBlockSignatures::isFaninNode(BasicBlock * const BB) {
    return faninBlocks.count(BB);
  }


  bool AssignBlockSignatures::hasFaninSuccessor(BasicBlock * const BB) {
    return faninSuccessors.count(BB);
  }


  char AssignBlockSignatures::ID = 0;
}

static RegisterPass<cfcss::AssignBlockSignatures>
    X("assign-block-signatures", "Assign Block Signatures (CFCSS)");

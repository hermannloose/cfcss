/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

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

static const char *debugPrefix = "AssignBlockSignatures: ";

namespace cfcss {

  typedef std::pair<BasicBlock*, ConstantInt*> SignatureEntry;
  typedef std::pair<BasicBlock*, BasicBlock*> BlockEntry;

  AssignBlockSignatures::AssignBlockSignatures() :
      ModulePass(ID), blockSignatures(), signatureUpdateSources(),
      adjustFor(), faninBlocks(), faninSuccessors(), nextID(0) {
  }


  void AssignBlockSignatures::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }


  bool AssignBlockSignatures::runOnModule(Module &M) {
    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      if (fi->isDeclaration()) {
        DEBUG(errs() << debugPrefix << "Skipping [" << fi->getName() << "], is a declaration.\n");
        continue;
      }

      DEBUG(errs() << debugPrefix << "Running on [" << fi->getName() << "].\n");

      for (Function::iterator i = fi->begin(), e = fi->end(); i != e; ++i) {
        blockSignatures.insert(SignatureEntry(i, ConstantInt::get(
            Type::getInt64Ty(getGlobalContext()), nextID)));

        DEBUG(errs() << debugPrefix << "[" << i->getName()
            << "] has signature " << nextID << ".\n");

        ++nextID;

        if (pred_begin(i) == pred_end(i)) {
          // i.e. the entry block of a function.
          continue;
        }

        // Find fanin nodes and track their predecessors for efficient lookup
        // later on.
        if (++pred_begin(i) != pred_end(i)) {
          faninBlocks.insert(i);
          for (pred_iterator pred_i = pred_begin(i), pred_e = pred_end(i);
              pred_i != pred_e; ++pred_i) {

            faninSuccessors.insert(*pred_i);
          }
        }

        // Determine authoritative predecessors for each block. This is used in
        // calculating signature updates and adjustments during instrumentation.
        if (faninBlocks.count(i)) {
          BasicBlock *authoritativePredecessor = NULL;
          for (pred_iterator pred_i = pred_begin(i), pred_e = pred_end(i);
              pred_i != pred_e; ++pred_i) {

            // This should be the same for all predecessors. They've either all
            // been touched in an earlier iteration or ignored in the
            // else-branch.
            if (!(adjustFor.lookup(*pred_i))) {
              if (!authoritativePredecessor) {
                // Pick the first predecessor we get as the authoritative one.
                authoritativePredecessor = *pred_i;
                DEBUG(errs() << debugPrefix << "[" << (*pred_i)->getName()
                    << "] is authoritative predecessor for [" << i->getName() << "].\n");
              }

              adjustFor.insert(BlockEntry(*pred_i, authoritativePredecessor));
              DEBUG(errs() << debugPrefix << "[" << (*pred_i)->getName()
                  << "] will adjust signature for ["
                  << authoritativePredecessor->getName() << "].\n");
            }
          }
        } else {
          pred_iterator singlePred = pred_begin(i);
          // If this block won't be touched by later iterations, set its
          // authoritative sibling to itself for uniform treatment.
          if (!adjustFor.lookup(*singlePred) && !faninSuccessors.count(*singlePred)) {
            DEBUG(errs() << debugPrefix << "[" << i->getName() << "] has no fanin successors.\n");
            adjustFor.insert(BlockEntry(*singlePred, *singlePred));
          }
        }
      }

      DEBUG(errs() << debugPrefix << "Finished on [" << fi->getName() << "].\n");
    }

    return false;
  }


  ConstantInt* AssignBlockSignatures::getSignature(BasicBlock * const BB) {
    return blockSignatures.lookup(BB);
  }


  BasicBlock* AssignBlockSignatures::getAuthoritativePredecessor(BasicBlock * const BB) {
    return adjustFor.lookup(*pred_begin(BB));
  }


  BasicBlock* AssignBlockSignatures::getAuthoritativeSibling(BasicBlock * const BB) {
    return adjustFor.lookup(BB);
  }


  void AssignBlockSignatures::notifyAboutSplitBlock(BasicBlock * const head,
      BasicBlock * const tail) {

    adjustFor.insert(BlockEntry(tail, adjustFor.lookup(head)));
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
    X("assign-block-sigs", "Assign block signatures (CFCSS)", false, true);

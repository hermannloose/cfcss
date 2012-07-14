/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "AssignBlockSignatures.h"
#include "RemoveCFGAliasing.h"

#include "llvm/ADT/APInt.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace cfcss {

  typedef std::pair<BasicBlock*, ConstantInt*> SignatureEntry;
  typedef std::pair<BasicBlock*, bool> FaninEntry;
  typedef std::pair<BasicBlock*, BasicBlock*> BlockEntry;

  AssignBlockSignatures::AssignBlockSignatures() :
      FunctionPass(ID), blockSignatures(), signatureUpdateSources(),
      adjustFor(), blockFanin() {

    nextID = 0;
  }


  void AssignBlockSignatures::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<RemoveCFGAliasing>();
  }


  bool AssignBlockSignatures::runOnFunction(Function &F) {
    for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
      blockSignatures.insert(SignatureEntry(i, ConstantInt::get(
          Type::getInt64Ty(getGlobalContext()), nextID)));

      DEBUG(errs() << "[" << i->getName() << "] has signature " << nextID << ".\n");
      ++nextID;

      int predecessors = 0;
      BasicBlock *authoritativePredecessor = NULL;
      for (pred_iterator pred_i = pred_begin(i), pred_e = pred_end(e);
          pred_i != pred_e; ++pred_i) {

        /**
         * The authoritative predecessor is determined once per fanin node,
         * for the set of its predecessors. The dealiasing pass required by
         * this pass ensures that multiple fanin nodes share none or all of
         * their predecessors.
         */
        if (!(adjustFor.lookup(*pred_i))) {
          if (!authoritativePredecessor) {
            authoritativePredecessor = *pred_i;
          } else {
            // Keep this for convenient lookup later on.
            faninSuccessors.insert(FaninEntry(*pred_i, true));
          }
          adjustFor.insert(BlockEntry(*pred_i, authoritativePredecessor));
        }

        ++predecessors;
      }

      blockFanin.insert(FaninEntry(i, (predecessors > 1)));
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


  bool AssignBlockSignatures::isFaninNode(BasicBlock * const BB) {
    return blockFanin.lookup(BB);
  }


  bool AssignBlockSignatures::hasFaninSuccessor(BasicBlock * const BB) {
    return faninSuccessors.lookup(BB);
  }


  char AssignBlockSignatures::ID = 0;
}

static RegisterPass<cfcss::AssignBlockSignatures>
    X("assign-block-sigs", "Assign block signatures for CFCSS", false, true);

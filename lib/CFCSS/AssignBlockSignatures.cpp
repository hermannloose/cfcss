/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "AssignBlockSignatures.h"

#include "llvm/ADT/APInt.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CFG.h"

namespace cfcss {

  typedef std::pair<BasicBlock*, ConstantInt*> SignatureEntry;
  typedef std::pair<BasicBlock*, bool> FaninEntry;

  AssignBlockSignatures::AssignBlockSignatures() :
      FunctionPass(ID), blockSignatures(), signatureUpdateSources(), blockFanin() {

    nextID = 0;
  }

  void AssignBlockSignatures::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }

  bool AssignBlockSignatures::runOnFunction(Function &F) {
    for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
      blockSignatures.insert(SignatureEntry(i, ConstantInt::get(
          Type::getInt64Ty(getGlobalContext()), nextID)));

      ++nextID;

      int predecessors = 0;
      for (pred_iterator pred_i = pred_begin(i), pred_e = pred_end(e);
          pred_i != pred_e; ++pred_i) {

        ++predecessors;
        if (predecessors > 1) {
          break;
        }
      }

      blockFanin.insert(FaninEntry(i, (predecessors > 1)));
    }

    return false;
  }

  // FIXME(hermannloose): This is a dirty hack!
  ConstantInt* AssignBlockSignatures::getSignature(BasicBlock * const BB) {
    ConstantInt *signature = blockSignatures.lookup(BB);
    /*
    if (!signature) {
      signature = ConstantInt::get(Type::getInt64Ty(getGlobalContext()), nextID);
      blockSignatures.insert(SignatureEntry(BB, signature));
      ++nextID;
    }
    */

    return signature;
  }

  bool AssignBlockSignatures::isFaninNode(BasicBlock * const BB) {
    return blockFanin.lookup(BB);
  }

  char AssignBlockSignatures::ID = 0;
}

static RegisterPass<cfcss::AssignBlockSignatures>
    X("assign-block-sigs", "Assign block signatures for CFCSS", false, true);

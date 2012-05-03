/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "AssignBlockSignatures.h"

#include "llvm/ADT/APInt.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/LLVMContext.h"

namespace cfcss {

  typedef std::pair<BasicBlock*, Constant*> SignatureEntry;

  AssignBlockSignatures::AssignBlockSignatures() :
      FunctionPass(ID), blockSignatures() {

    nextID = 0;
  }

  void AssignBlockSignatures::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }

  bool AssignBlockSignatures::runOnFunction(Function &F) {
    for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
      blockSignatures.insert(SignatureEntry(i, Constant::getIntegerValue(
          IntegerType::get(getGlobalContext(), 64),
          APInt(64, nextID))));

      ++nextID;
    }

    return false;
  }

  Constant* AssignBlockSignatures::getSignature(BasicBlock &BB) {
    return blockSignatures.lookup(&BB);
  }

  char AssignBlockSignatures::ID = 0;
}

static RegisterPass<cfcss::AssignBlockSignatures>
    X("assign-block-sigs", "Assign block signatures for CFCSS", false, true);

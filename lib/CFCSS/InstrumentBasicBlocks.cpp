/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "AssignBlockSignatures.h"
#include "InstrumentBasicBlocks.h"

#include "llvm/Function.h"

using namespace llvm;

namespace cfcss {

  InstrumentBasicBlocks::InstrumentBasicBlocks() : FunctionPass(ID) {}

  void InstrumentBasicBlocks::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AssignBlockSignatures>();
  }

  bool InstrumentBasicBlocks::runOnFunction(Function &F) {
    AssignBlockSignatures &ABS = getAnalysis<AssignBlockSignatures>();

    for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
      BasicBlock &BB = *i;

      Constant* signature = ABS.getSignature(BB);

      signature->dump();
    }

    return true;
  }

  char InstrumentBasicBlocks::ID = 0;
}

static RegisterPass<cfcss::InstrumentBasicBlocks>
    X("instrument-blocks", "Instrument basic blocks with CFCSS signatures", false, false);

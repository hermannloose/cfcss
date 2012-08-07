/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "ReturnBlocks.h"

#include "RemoveCFGAliasing.h"
#include "SplitAfterCall.h"

#include "llvm/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

static const char *debugPrefix = "ReturnBlocks: ";

namespace cfcss {

  typedef std::pair<Function*, BlockSet*> ReturnBlocksEntry;


  ReturnBlocks::ReturnBlocks() : ModulePass(ID), returnBlocks() {}


  bool ReturnBlocks::runOnModule(Module &M) {
    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      BlockSet *returnBlocksInFunction = new BlockSet();

      for (Function::iterator i = fi->begin(), e = fi->end(); i != e; ++i) {
        if (isa<ReturnInst>(i->getTerminator())) {
          DEBUG(errs() << debugPrefix << "[" << i->getName() << "] in [" << fi->getName()
              << "] is a return block.\n");

          returnBlocksInFunction->insert(i);
        }
      }

      returnBlocks.insert(ReturnBlocksEntry(fi, returnBlocksInFunction));
    }

    return false;
  }


  void ReturnBlocks::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }


  BlockSet* ReturnBlocks::getReturnBlocks(Function * const F) {
    return returnBlocks.lookup(F);
  }

  char ReturnBlocks::ID = 0;
}

static RegisterPass<cfcss::ReturnBlocks>
    X("return-blocks", "Find return blocks for each function (CFCSS)", true, true);

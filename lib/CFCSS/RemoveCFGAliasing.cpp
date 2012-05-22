/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "RemoveCFGAliasing.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace cfcss {

  STATISTIC(NumAliasingBlocks, "Number of aliasing basic blocks found");

  RemoveCFGAliasing::RemoveCFGAliasing() : FunctionPass(ID) {}

  bool RemoveCFGAliasing::runOnFunction(Function &F) {
    for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
      if (doesAlias(i)) {
        DEBUG(errs() << i->getName().str() << " does alias.\n");
      }
    }
    return false;
  }

  void RemoveCFGAliasing::getAnalysisUsage(AnalysisUsage &AU) const {

  }

  bool RemoveCFGAliasing::doesAlias(BasicBlock *BB) {
    DEBUG(errs() << "Checking [" << BB->getName().str() << "] for aliasing.\n");

    // The aliasing problem is limited to fanin nodes.
    if (!BB->hasNUsesOrMore(2)) {
      DEBUG(errs() << "[" << BB->getName().str() << "] is not a fanin node, can't alias.\n");

      return false;
    }

    SmallPtrSet<BasicBlock*, 16> *predecessors =
        new SmallPtrSet<BasicBlock*, 16>(pred_begin(BB), pred_end(BB));

    DEBUG(errs() << "Predecessors:");
    for (SmallPtrSet<BasicBlock*, 16>::iterator i = predecessors->begin(), e = predecessors->end();
        i != e; ++i) {

      DEBUG(errs() << " [" << (*i)->getName().str() << "]");
    }
    DEBUG(errs() << "\n");

    for (pred_iterator i = pred_begin(BB), e = pred_end(BB); i != e; ++i) {
      DEBUG(errs() << "Checking successors of [" << (*i)->getName().str() << "]\n");

      for (succ_iterator succ_i = succ_begin(*i), succ_e = succ_end(*i);
          succ_i != succ_e; ++succ_i) {

        if (*succ_i == BB) { continue; }

        // The aliasing problem is limited to fanin nodes.
        if (succ_i->hasNUsesOrMore(2)) {
          DEBUG(errs() << "[" << succ_i->getName().str() << "] is a fanin node, checking for "
              << "predecessor overlap.\n");

          for (pred_iterator pred_i = pred_begin(*succ_i), pred_e = pred_end(*succ_i);
              pred_i != pred_e; ++pred_i) {

            if (!predecessors->count(*pred_i)) {
              DEBUG(errs() << "[" << (*pred_i)->getName().str() << "] is a predecessor of ["
                  << (*succ_i)->getName().str() << "] but not [" << BB->getName().str() << "]\n");

              ++NumAliasingBlocks;

              return true;
            }
          }
        }
      }
    }

    return false;
  }

  char RemoveCFGAliasing::ID = 0;

}

static RegisterPass<cfcss::RemoveCFGAliasing>
    X("remove-cfg-aliasing", "Remove CFG aliasing regarding CFCSS", false, false);

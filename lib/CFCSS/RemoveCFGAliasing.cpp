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
      BlockMap *aliasingBlocks = getAliasingBlocks(i);
      NumAliasingBlocks += aliasingBlocks->size();

      for (BlockMap::iterator via_i = aliasingBlocks->begin(), via_e = aliasingBlocks->end();
          via_i != via_e; ++via_i) {

        for (BlockSet::iterator alias_i = via_i->second->begin(), alias_e = via_i->second->end();
            alias_i != alias_e; ++alias_i) {

          DEBUG(errs() << "[" << i->getName().str() << "]\n  ["
              << (*alias_i)->getName().str() << "] via ["
              << via_i->first->getName().str() << "]\n");
        }
      }

      delete aliasingBlocks;
    }

    return false;
  }

  void RemoveCFGAliasing::getAnalysisUsage(AnalysisUsage &AU) const {

  }

  RemoveCFGAliasing::BlockMap* RemoveCFGAliasing::getAliasingBlocks(BasicBlock *BB) {
    DEBUG(errs() << "Checking [" << BB->getName().str() << "] for aliasing.\n");

    BlockMap *aliasingBlocks = new BlockMap();

    // FIXME(hermannloose): Might make control-flow a bit nicer here.
    // The aliasing problem is limited to fanin nodes.
    if (!BB->hasNUsesOrMore(2)) {
      DEBUG(errs() << "[" << BB->getName().str() << "] is not a fanin node, can't alias.\n");

      return aliasingBlocks;
    }

    BlockSet *predecessors = new BlockSet(pred_begin(BB), pred_end(BB));

    DEBUG(errs() << "Predecessors:");
    for (BlockSet::iterator i = predecessors->begin(), e = predecessors->end();
        i != e; ++i) {

      DEBUG(errs() << " [" << (*i)->getName().str() << "]");
    }
    DEBUG(errs() << "\n");

    for (pred_iterator i = pred_begin(BB), e = pred_end(BB); i != e; ++i) {
      DEBUG(errs() << "Checking successors of [" << (*i)->getName().str() << "]\n");

      BlockSet *aliasingSuccessors = new BlockSet();

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

              aliasingSuccessors->insert(*succ_i);
              break;
            }
          }
        }
      }

      if (!aliasingSuccessors->empty()) {
        aliasingBlocks->insert(std::pair<BasicBlock*, BlockSet*>(*i, aliasingSuccessors));
      } else {
        delete aliasingSuccessors;
      }
    }

    return aliasingBlocks;
  }

  char RemoveCFGAliasing::ID = 0;

}

static RegisterPass<cfcss::RemoveCFGAliasing>
    X("remove-cfg-aliasing", "Remove CFG aliasing regarding CFCSS", false, false);

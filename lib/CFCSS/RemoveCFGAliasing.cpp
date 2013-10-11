#define DEBUG_TYPE "cfcss-remove-cfg-aliasing"

#include "RemoveCFGAliasing.h"

#include "GatewayFunctions.h"
#include "InstructionIndex.h"
#include "SplitAfterCall.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static const char *debugPrefix = "RemoveCFGAliasing: ";

namespace cfcss {

  STATISTIC(NumAliasingBlocks, "Number of aliasing basic blocks found");
  STATISTIC(NumProxyBlocks, "Number of proxy blocks inserted");

  RemoveCFGAliasing::RemoveCFGAliasing() : ModulePass(ID) {}

  bool RemoveCFGAliasing::runOnModule(Module &M) {
    bool modifiedCFG = false;

    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      if (fi->isDeclaration()) {
        DEBUG(errs() << debugPrefix << "Skipping [" << fi->getName() << "], is a declaration.\n");
        continue;
      }

      DEBUG(errs() << debugPrefix << "Running on [" << fi->getName() << "].\n");

      for (Function::iterator i = fi->begin(), e = fi->end(); i != e; ++i) {
        BlockToBlockSetMap *aliasingBlocks = getAliasingBlocks(i);
        if (aliasingBlocks->size()) {
          NumAliasingBlocks += aliasingBlocks->size() + 1;
          // We will modify CFG during this run.
          modifiedCFG = true;
        }

        for (BlockToBlockSetMap::iterator via_i = aliasingBlocks->begin(),
            via_e = aliasingBlocks->end(); via_i != via_e; ++via_i) {

          for (BlockSet::iterator alias_i = via_i->second->begin(), alias_e = via_i->second->end();
              alias_i != alias_e; ++alias_i) {

            DEBUG(
              errs() << debugPrefix;
              errs().indent(2).changeColor(raw_ostream::WHITE, true /* bold */);
              errs() << "[" << i->getName() << "] and [" << (*alias_i)->getName() << "] via ["
                  << via_i->first->getName() << "]\n";
              errs().resetColor();
            );

            insertProxyBlock(via_i->first, *alias_i);
            ++NumProxyBlocks;
          }
        }

        delete aliasingBlocks;
      }

      DEBUG(
        errs() << debugPrefix;
        errs().changeColor(raw_ostream::GREEN);
        errs() << "Finished on [" << fi->getName() << "].\n";
        errs().resetColor();
      );
    }

    return modifiedCFG;
  }

  void RemoveCFGAliasing::getAnalysisUsage(AnalysisUsage &AU) const {
    // TODO(hermannloose): AU.setPreservesAll() would probably not hurt.
    AU.addPreserved<GatewayFunctions>();
    AU.addPreserved<InstructionIndex>();
    AU.addPreserved<SplitAfterCall>();
  }

  BlockToBlockSetMap* RemoveCFGAliasing::getAliasingBlocks(BasicBlock *BB) {
    BlockToBlockSetMap *aliasingBlocks = new BlockToBlockSetMap();

    // FIXME(hermannloose): Might make control-flow a bit nicer here.
    // The aliasing problem is limited to fanin nodes.
    if (!BB->hasNUsesOrMore(2)) {
      return aliasingBlocks;
    }

    BlockSet *predecessors = new BlockSet(pred_begin(BB), pred_end(BB));

    for (pred_iterator i = pred_begin(BB), e = pred_end(BB); i != e; ++i) {
      BlockSet *aliasingSuccessors = new BlockSet();

      for (succ_iterator succ_i = succ_begin(*i), succ_e = succ_end(*i);
          succ_i != succ_e; ++succ_i) {

        if (*succ_i == BB) { continue; }

        // The aliasing problem is limited to fanin nodes.
        if (succ_i->hasNUsesOrMore(2)) {
          for (pred_iterator pred_i = pred_begin(*succ_i), pred_e = pred_end(*succ_i);
              pred_i != pred_e; ++pred_i) {

            // We only check whether the predecessors of succ_i are a subset of
            // the predecessors of BB. Aliasing does occur in the superset case
            // as well, which will be detected when running this method on
            // succ_i later on.
            if (!predecessors->count(*pred_i)) {
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

  // FIXME(hermannloose): Assert that target is in fact a successor of source!
  BasicBlock* RemoveCFGAliasing::insertProxyBlock(BasicBlock *source, BasicBlock *target) {
    BasicBlock *proxyBlock = BasicBlock::Create(
        getGlobalContext(),
        "proxyBlock",
        source->getParent());

    BranchInst::Create(target, proxyBlock);

    // We can't use replaceSuccessorsPhiUsesWith(), as we only want to change
    // target and not all successors at once.
    PHINode *phiNode = NULL;
    for (BasicBlock::iterator ii = target->begin(); (phiNode = dyn_cast<PHINode>(ii)); ++ii) {
      // In the presence of switch statements, multiple edges may exist between a source and
      // a target, with a corresponding number of operands in the phi node referring to the same
      // basic block. Since these edges are replaced by a single one from the proxy block to the
      // target block, only one operand referring to the proxy block may remain in the phi node.

      Value *value = phiNode->getIncomingValue(phiNode->getBasicBlockIndex(source));

      // Unfortunately BasicBlock::removePredecessor() does not work as advertised and only removes
      // the first matching operand each time it is called, so we have to do this by hand.
      while (true) {
        int idx = phiNode->getBasicBlockIndex(source);
        if (idx >= 0) {
          phiNode->removeIncomingValue(idx, false);
        } else {
          break;
        }
      }

      phiNode->addIncoming(value, proxyBlock);
    }

    TerminatorInst *terminator = source->getTerminator();
    for (unsigned int idx = 0; idx < terminator->getNumSuccessors(); ++idx) {
      if (terminator->getSuccessor(idx) == target) {
        terminator->setSuccessor(idx, proxyBlock);
      }
    }

    return proxyBlock;
  }

  char RemoveCFGAliasing::ID = 0;

}

static RegisterPass<cfcss::RemoveCFGAliasing>
    X("remove-cfg-aliasing", "Remove CFG aliasing (CFCSS)");

/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "AuthoritativeCallsites.h"

#include "AssignBlockSignatures.h"
#include "RemoveCFGAliasing.h"
#include "ReturnBlocks.h"
#include "SplitAfterCall.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

static const char *debugPrefix = "AuthoritativeCallsites: ";

static const char *yellow = "\x1b[33m";
static const char *reset = "\x1b[0m";

namespace cfcss {

  typedef std::pair<Function*, CallSite*> CallsiteEntry;

  AuthoritativeCallsites::AuthoritativeCallsites() : FunctionPass(ID), authoritativeCallsites() {

  }

  void AuthoritativeCallsites::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }

  bool AuthoritativeCallsites::runOnFunction(Function &F) {
    DEBUG(errs() << debugPrefix << "Running on [" << F.getName() << "].\n");

    for (Function::iterator fi = F.begin(), fe = F.end(); fi != fe; ++fi) {
      for (BasicBlock::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
        // TODO(hermannloose): Handle InvokeInst.
        if (CallInst *call = dyn_cast<CallInst>(bi)) {
          if (Function *called = call->getCalledFunction()) {
            if (!(called->isDeclaration() || called->isIntrinsic())) {
              if (!authoritativeCallsites.count(called)) {
                DEBUG(errs() << debugPrefix << "(in [" << F.getName() << "]) "
                    << "Adding authoritative callsite for [" << called->getName() << "]:\n");
                DEBUG(call->dump());

                authoritativeCallsites.insert(
                    CallsiteEntry(called, new CallSite(call)));
              }
            } else {
              DEBUG(errs() << debugPrefix << "[" << called->getName() << "] "
                  << "is declaration or intrinsic, ignoring.\n");
            }
          }
        }
      }
    }

    return false;
  }

  CallSite* AuthoritativeCallsites::getAuthoritativeCallsite(Function * const F) {
    return authoritativeCallsites.lookup(F);
  }

  char AuthoritativeCallsites::ID = 0;
}

static RegisterPass<cfcss::AuthoritativeCallsites>
    X("authoritative-callsites", "Authoritative Call Sites (CFCSS)");

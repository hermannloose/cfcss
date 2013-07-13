/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "RemoveCallGraphAliasing.h"

namespace cfcss {

  RemoveCallGraphAliasing::RemoveCallGraphAliasing() : CallGraphSCCPass(ID) {}

  bool RemoveCallGraphAliasing::runOnSCC(CallGraphSCC &SCC) {
    return false;
  }

  void RemoveCallGraphAliasing::getAnalysisUsage(AnalysisUsage &AU) const {

  }

  char RemoveCallGraphAliasing::ID = 0;

}

static RegisterPass<cfcss::RemoveCallGraphAliasing>
    X("remove-callgraph-aliasing", "Remove call-graph aliasing (CFCSS)",
    false, false);

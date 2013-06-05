/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/Analysis/CallGraphSCCPass.h"

using namespace llvm;

namespace cfcss {

  class RemoveCallGraphAliasing : public CallGraphSCCPass {
    public:
      static char ID;

      RemoveCallGraphAliasing();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnSCC(CallGraphSCC &SCC);
  };

}

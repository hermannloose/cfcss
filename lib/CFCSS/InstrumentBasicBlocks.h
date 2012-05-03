/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {

  class InstrumentBasicBlocks : public FunctionPass {
    public:
      static char ID;

      InstrumentBasicBlocks();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnFunction(Function &F);
  };

}

/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {

  class RemoveCFGAliasing : public FunctionPass {
    public:
      static char ID;

      RemoveCFGAliasing();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnFunction(Function &F);

    private:
      bool doesAlias(BasicBlock *BB);
  };

}

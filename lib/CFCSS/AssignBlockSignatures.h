/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/Constant.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {
  typedef DenseMap<BasicBlock*, Constant*> SignatureMap;

  class AssignBlockSignatures : public FunctionPass {
    public:
      static char ID;

      AssignBlockSignatures();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnFunction(Function &F);

      Constant* getSignature(BasicBlock &BB);

    private:
      SignatureMap blockSignatures;
      unsigned long nextID;
  };

}

/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "AssignBlockSignatures.h"

#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {
  // FIXME(hermannloose): Remove duplication.
  typedef DenseMap<BasicBlock*, ConstantInt*> SignatureMap;

  class InstrumentBasicBlocks : public FunctionPass {

    public:
      static char ID;

      InstrumentBasicBlocks();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnFunction(Function &F);

    private:
      SignatureMap splitBlocks;

      ConstantInt* getSignature(BasicBlock * const BB,
          AssignBlockSignatures &ABS);
  };

}

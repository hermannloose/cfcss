/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/IR/Constants.h"

using namespace llvm;

namespace cfcss {

  class AssignFunctionSignatures : public CallGraphSCCPass {
    public:
      static char ID;

      AssignFunctionSignatures();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnSCC(CallGraphSCC &SCC);

      ConstantInt* getSignature(Function * const F);
      Function* getAuthoritativePredecessor(Function * const F);

    private:
      typedef DenseMap<Function*, ConstantInt*> SignatureMap;
      typedef DenseMap<Function*, Function*> PredecessorMap;

      SignatureMap functionSignatures;
      PredecessorMap authoritativePredecessors;
      unsigned long nextID;
  };

}

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

  /**
   * Assign each function a unique ID. This will also pick an authoritative
   * predecessor for every callee that is the target of multiple call
   * instructions elsewhere.
   */
  class AssignFunctionSignatures : public CallGraphSCCPass {
    public:
      static char ID;

      AssignFunctionSignatures();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnSCC(CallGraphSCC &SCC);

      /**
       * Get the signature of the given function.
       */
      ConstantInt* getSignature(Function * const F);

      /**
       * Get the authoritative predecessor for the given function.
       */
      Function* getAuthoritativePredecessor(Function * const F);

    private:
      typedef DenseMap<Function*, ConstantInt*> SignatureMap;
      typedef DenseMap<Function*, Function*> PredecessorMap;

      SignatureMap functionSignatures;
      PredecessorMap authoritativePredecessors;
      unsigned long nextID;
  };

}

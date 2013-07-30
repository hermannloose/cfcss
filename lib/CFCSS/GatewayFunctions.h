/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {

  typedef DenseMap<Function*, Function*> FunctionMap;

  /**
   * TODO(hermannloose): Document this.
   */
  class GatewayFunctions : public ModulePass {
    public:
      static char ID;

      GatewayFunctions();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module &M);

      /**
       * Check whether a function is a gateway, used in instrumentation.
       */
      bool isGateway(Function * const F);

      /**
       * Get the authoritative predecessor of a function, after gateways have
       * been inserted.
       */
      Function* getAuthoritativePredecessor(Function * const F);

    private:
      FunctionMap authoritativePredecessors;
      FunctionMap gatewayToInternal;
  };

}

/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "Common.h"

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace cfcss {

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
       * Get the internal function that the given gateway call through to.
       */
      Function* getInternalFunction(Function * const F);

      /**
       * Get the authoritative predecessor of a function, after gateways have
       * been inserted.
       */
      Function* getAuthoritativePredecessor(Function * const F);

      /**
       * Check whether the given function is a fanin node.
       */
      bool isFaninNode(Function * const F);

    private:
      FunctionToFunctionMap authoritativePredecessors;
      FunctionToFunctionMap gatewayToInternal;
      FunctionSet faninNodes;
  };

}

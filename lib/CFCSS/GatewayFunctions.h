/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "Common.h"

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace cfcss {

  /**
   * TODO(hermannloose): Document this.
   */
  class GatewayFunctions : public llvm::ModulePass {
    public:
      static char ID;

      GatewayFunctions();

      virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
      virtual bool runOnModule(llvm::Module &M);

      /**
       * Check whether a function is a gateway, used in instrumentation.
       */
      bool isGateway(llvm::Function * const F);

      /**
       * Get the internal function that the given gateway call through to.
       */
      llvm::Function* getInternalFunction(llvm::Function * const F);

      /**
       * Get the authoritative predecessor of a function, after gateways have been inserted.
       */
      llvm::Function* getAuthoritativePredecessor(llvm::Function * const F);

      /**
       * Check whether the given function is a fanin node.
       */
      bool isFaninNode(llvm::Function * const F);

    private:
      FunctionToFunctionMap authoritativePredecessors;
      FunctionToFunctionMap gatewayToInternal;
      FunctionSet faninNodes;
  };

}

#pragma once

#include "Common.h"

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace cfcss {

  /**
   * Wrap externally visible functions with a gateway function to later establish a known good
   * state for CFCSS regarding the contents of GSR and D.
   *
   * For all externally visible functions, this will rename the original function and change it to
   * have internal linkage. It then adds a new externally visible function with the same signature
   * as a gateway that forwards calls to the private implementation and returns the result.
   * Instrumentation of the gateway will later concern itself with setting GSR and D to sensible
   * starting values. All calls from within the module to the original function bypass the gateway
   * and go directly to the implementation, since they will carry actual signatures in GSR and
   * D that we want to check upon entering the called function.
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

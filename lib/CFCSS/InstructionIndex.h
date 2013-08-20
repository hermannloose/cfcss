/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "Common.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace cfcss {

  typedef std::list<llvm::CallInst*> CallList;
  typedef std::list<llvm::ReturnInst*> ReturnList;

  /**
   * Gather interesting instructions—i.e. calls, invokes and returns—for lookup
   * by other CFCSS passes.
   */
  class InstructionIndex : public llvm::ModulePass {

    public:
      static char ID;

      InstructionIndex();

      virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
      virtual bool runOnModule(llvm::Module &M);

      /**
       * Get a list of all call instructions contained in the given function.
       */
      CallList* getCalls(llvm::Function * const F);

      /**
       * Get the primary call instruction referring to the given target,
       * contained within the given function.
       */
      llvm::CallInst* getPrimaryCallTo(llvm::Function * const target,
          llvm::Function * const container);

      /**
       * Check whether the function has any return instructions.
       *
       * LLVM's Function::doesNotReturn() is not always true for functions
       * where every code path contains a call to a function that does not
       * return, i.e. this information is not propagated as expected. We use
       * this as a workaround to not split basic blocks after calls to such
       * functions.
       *
       * This is equivalent to getReturns()->empty().
       */
      bool doesNotReturn(llvm::Function * const F);

      /**
       * Get a list of all return instructions contained in the given function.
       */
      ReturnList* getReturns(llvm::Function * const F);

      /**
       * Get the primary return instruction of the given function.
       *
       * This is used in calculating signature adjustments.
       */
      llvm::ReturnInst* getPrimaryReturn(llvm::Function * const F);

    private:
      llvm::DenseMap<llvm::Function*, CallList*> callsByFunction;

      typedef llvm::DenseMap<llvm::Function*, llvm::CallInst*> PrimaryCallMap;
      llvm::DenseMap<llvm::Function*, PrimaryCallMap*> primaryCallsByFunction;

      llvm::DenseMap<llvm::Function*, ReturnList*> returnsByFunction;
  };

}

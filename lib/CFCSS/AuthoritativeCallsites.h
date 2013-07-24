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
#include "llvm/Support/CallSite.h"

using namespace llvm;

namespace cfcss {

  typedef DenseMap<Function*, CallSite*> CallsiteMap;

  class AuthoritativeCallsites : public FunctionPass {
    public:
      static char ID;

      AuthoritativeCallsites();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnFunction(Function &F);

      CallSite* getAuthoritativeCallsite(Function * const F);

    private:
      CallsiteMap authoritativeCallsites;
  };
}

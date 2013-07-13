#include "llvm/Analysis/LoopInfo.h"

using namespace llvm;

struct SegfaultPass : public ModulePass {
  typedef ModulePass super;

  static char ID;
  SegfaultPass() :
    super(ID) {
    }

  virtual bool runOnModule(Module &M) {
    return false;
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addPreserved<LoopInfo>();
    AU.addRequiredTransitive<LoopInfo>();
  }
};

char SegfaultPass::ID = 0;
static RegisterPass<SegfaultPass> X("segfault", "segfault", false, true);

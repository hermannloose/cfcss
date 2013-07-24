/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "GatewayFunctions.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

static const char *debugPrefix = "GatewayFunctions: ";

static const char *yellow = "\x1b[33m";
static const char *reset = "\x1b[0m";

namespace cfcss {

  typedef std::pair<Function*, Function*> GatewayToInternalEntry;

  GatewayFunctions::GatewayFunctions() : ModulePass(ID), gatewayToInternal() {

  }

  void GatewayFunctions::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<CallGraph>();
  }

  bool GatewayFunctions::runOnModule(Module &M) {
    bool modified = false;

    CallGraph &CG = getAnalysis<CallGraph>();

    CallGraphNode *externalCallers = CG.getExternalCallingNode();

    for (CallGraphNode::iterator ci = externalCallers->begin(), ce = externalCallers->end();
        ci != ce; ++ci) {

      CallGraphNode *externallyCalled = ci->second;

      if (Function *F = externallyCalled->getFunction()) {
        if (F->isIntrinsic()) {
          DEBUG(errs() << debugPrefix << yellow
              << "Ignoring [" << F->getName() << "] (intrinsic)\n" << reset);

          continue;
        }

        if (F->isDeclaration()) {
          DEBUG(errs() << debugPrefix << yellow
              << "Ignoring [" << F->getName() << "] (declaration)\n" << reset);

          continue;
        }

        if (externallyCalled->getNumReferences() < 2) {
          DEBUG(errs() << debugPrefix << yellow
              << "Ignoring [" << F->getName() << "] (no internal callers)\n" << reset);

          continue;
        }

        DEBUG(errs() << debugPrefix
            << "Creating gateway function for [" << F->getName() << "].\n");

        ValueToValueMapTy vmap;
        Function *internal = CloneFunction(F, vmap, false);

        internal->setName(F->getName() + "_internal");
        internal->setLinkage(GlobalValue::InternalLinkage);
        M.getFunctionList().push_back(internal);

        CallGraphNode *internalNode = CG.getOrInsertFunction(internal);

        // Add call graph edges for calls in the cloned function.
        // TODO(hermannloose): Deal with InvokeInst.
        for (Function::iterator fi = internal->begin(), fe = internal->end(); fi != fe; ++fi) {
          for (BasicBlock::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
            if (CallInst *call = dyn_cast<CallInst>(bi)) {
              if (Function *calledFunction = call->getCalledFunction()) {
                if (!calledFunction->isIntrinsic()) {
                  internalNode->addCalledFunction(CallSite(call), CG[calledFunction]);
                }
              } else {
                DEBUG(errs() << debugPrefix << yellow
                    << "Not a normal function call, ignoring:\n" << reset);

                DEBUG(call->dump());
              }
            }
          }
        }

        gatewayToInternal.insert(GatewayToInternalEntry(F, internal));

        if (internal->isDeclaration()) {
          DEBUG(errs() << debugPrefix << "[" << internal->getName() << "] is a declaration.\n");
        }
        assert(!internal->hasExternalLinkage() && "Should have internal linkage.");

        modified = true;
      } else {
        assert(false && "The external calling node should only call actual functions.");
      }
    }

    for (CallGraph::iterator ci = CG.begin(), ce = CG.end(); ci != ce; ++ci) {
      if (const Function *callerFunction = ci->first) {
        CallGraphNode *caller = ci->second;

        for (CallGraphNode::iterator callee_i = caller->begin(), callee_e = caller->end();
            callee_i != callee_e; ++callee_i) {

          CallGraphNode *callee = callee_i->second;

          if (Function *calleeFunction = callee->getFunction()) {
            if (gatewayToInternal.count(calleeFunction)) {
              DEBUG(errs() << debugPrefix << callerFunction->getName() << " calling "
                  << calleeFunction->getName() << " which is a gateway.\n");

              Function *internal = gatewayToInternal.lookup(calleeFunction);
              CallInst *callSite = dyn_cast<CallInst>(callee_i->first);

              callSite->setCalledFunction(internal);
            }
          }
        }
      } else {
        DEBUG(errs() << debugPrefix << yellow
            << "Ignoring call graph node without function.\n" << reset);
      }
    }

    return modified;
  }

  bool GatewayFunctions::isGateway(Function * const F) {
    return gatewayToInternal.count(F);
  }

  char GatewayFunctions::ID = 0;
}

static RegisterPass<cfcss::GatewayFunctions>
    X("gateway-functions", "Create gateways for externally visible functions, for use with CFCSS.",
        false, true);

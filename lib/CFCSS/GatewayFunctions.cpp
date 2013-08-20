/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss-gateway-functions"

#include "GatewayFunctions.h"

#include "llvm/ADT/SmallVector.h"
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

  GatewayFunctions::GatewayFunctions() : ModulePass(ID), authoritativePredecessors(),
      gatewayToInternal(), faninNodes() {

  }

  void GatewayFunctions::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<CallGraph>();
  }

  bool GatewayFunctions::runOnModule(Module &M) {
    bool modified = false;

    CallGraph &CG = getAnalysis<CallGraph>();

    CallGraphNode *externalCallers = CG.getExternalCallingNode();

    // Insert gateways for externally callable functions.
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
          // TODO(hermannloose): This is a dirty hack.
          // Probably primarily confusing due to the name. Documentation could
          // make clear, that both gateways and externally visible functions
          // without internal callers should be instrumented similarly, only
          // that newly created gateways are proxy functions without any
          // further instructions.
          DEBUG(errs() << debugPrefix << "[" << F->getName() << "] has no internal callers, "
              << "marking as gateway for easier handling.\n" << reset);

          gatewayToInternal.insert(FunctionToFunctionEntry(F, F));

          continue;
        }

        DEBUG(errs() << debugPrefix
            << "Creating gateway function for [" << F->getName() << "].\n");

        StringRef functionName = F->getName();

        F->setName(functionName + "_cfcss_internal");
        F->setLinkage(GlobalValue::InternalLinkage);

        externalCallers->removeAnyCallEdgeTo(externallyCalled);

        Function *gateway = dyn_cast<Function>(
            M.getOrInsertFunction(functionName, F->getFunctionType(), F->getAttributes()));

        assert(gateway);

        CallGraphNode *gatewayNode = CG.getOrInsertFunction(gateway);

        // TODO(hermannloose): Figure out how to do this in one line.
        std::vector<Value*> argumentVector;
        for (Function::arg_iterator ai = gateway->arg_begin(), ae = gateway->arg_end();
            ai != ae; ++ai) {
          argumentVector.push_back(ai);
        }

        BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", gateway);
        IRBuilder<> builder(entry);

        CallInst *forwardCall = builder.CreateCall(F, ArrayRef<Value*>(argumentVector));

        ReturnInst *forwardReturn = NULL;
        if (F->getReturnType()->isVoidTy()) {
          forwardReturn = builder.CreateRetVoid();
        } else {
          forwardReturn = builder.CreateRet(forwardCall);
        }

        DEBUG(entry->dump());

        gatewayNode->addCalledFunction(CallSite(forwardCall), externallyCalled);

        gatewayToInternal.insert(FunctionToFunctionEntry(gateway, F));

        modified = true;
      } else {
        assert(false && "The external calling node should only call actual functions.");
      }
    }

    // Determine authoritative predecessors.
    for (CallGraph::iterator ci = CG.begin(), ce = CG.end(); ci != ce; ++ci) {
      CallGraphNode *caller = ci->second;
      if (Function *callerFunction = caller->getFunction()) {
        for (CallGraphNode::iterator callee_i = caller->begin(), callee_e = caller->end();
            callee_i != callee_e; ++callee_i) {

          CallGraphNode *callee = callee_i->second;

          if (Function *calleeFunction = callee->getFunction()) {
            if (!authoritativePredecessors.count(calleeFunction)) {
              DEBUG(errs() << debugPrefix << "Setting [" << callerFunction->getName() << "] as "
                  << "authoritative predecessor of [" << calleeFunction->getName() << "].\n");

              authoritativePredecessors.insert(
                  FunctionToFunctionEntry(calleeFunction, callerFunction));
            }
          }
        }

        if (caller->getNumReferences() > 1) {
          faninNodes.insert(callerFunction);
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

  Function* GatewayFunctions::getInternalFunction(Function * const F) {
    return gatewayToInternal.lookup(F);
  }

  Function* GatewayFunctions::getAuthoritativePredecessor(Function * const F) {
    return authoritativePredecessors.lookup(F);
  }

  bool GatewayFunctions::isFaninNode(Function * const F) {
    return faninNodes.count(F);
  }

  char GatewayFunctions::ID = 0;
}

static RegisterPass<cfcss::GatewayFunctions> X("gateway-functions", "Gateway Functions (CFCSS)");

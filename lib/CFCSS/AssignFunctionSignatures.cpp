/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "AssignFunctionSignatures.h"

#include "llvm/LLVMContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace cfcss {

  typedef std::pair<Function*, ConstantInt*> SignatureEntry;
  typedef std::pair<Function*, Function*> PredecessorEntry;

  AssignFunctionSignatures::AssignFunctionSignatures() :
      CallGraphSCCPass(ID), functionSignatures(),
      authoritativePredecessors() {

    nextID = 0;
  }

  void AssignFunctionSignatures::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }

  bool AssignFunctionSignatures::runOnSCC(CallGraphSCC &SCC) {
    IntegerType *intType = Type::getInt64Ty(getGlobalContext());

    for (CallGraphSCC::iterator node_i = SCC.begin(), node_e = SCC.end();
        node_i != node_e; ++node_i) {

      CallGraphNode *node = *node_i;

      if (Function *F = node->getFunction()) {
        ConstantInt *signature = ConstantInt::get(intType, ++nextID);
        functionSignatures.insert(SignatureEntry(F, signature));

        DEBUG(errs() << F->getName().str() << " has signature "
            << signature->getValue().getLimitedValue() << ".\n");

        for (CallGraphNode::iterator callee_i = node->begin(),
            callee_e = node->end(); callee_i != callee_e; ++callee_i) {

          // Add ourselves as the authoritative predecessor to all callees that
          // do not yet have one.
          CallGraphNode *callee = callee_i->second;
          if (Function *calleeFunction = callee->getFunction()) {
            DEBUG(errs() << "Checking for authoritative predecessors of "
                << calleeFunction->getName().str() << ".\n");

            if (!authoritativePredecessors.lookup(calleeFunction)) {
              authoritativePredecessors.insert(PredecessorEntry(calleeFunction, F));
              DEBUG(errs() << "Set " << F->getName().str() << " as authoritative predecessor of "
                  << calleeFunction->getName().str() << ".\n");
            }
          }
        }
      } else {
        // Deal with an external node.
      }
    }

    return false;
  }

  ConstantInt* AssignFunctionSignatures::getSignature(Function * const F) {
    return functionSignatures.lookup(F);
  }

  Function* AssignFunctionSignatures::getAuthoritativePredecessor(Function * const F) {
    return authoritativePredecessors.lookup(F);
  }

  char AssignFunctionSignatures::ID = 0;

}

static RegisterPass<cfcss::AssignFunctionSignatures>
    X("assign-function-sigs", "Assign function signatures for CFCSS", false, true);

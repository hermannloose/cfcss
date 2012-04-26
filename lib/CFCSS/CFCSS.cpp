/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */

#define DEBUG_TYPE "cfcss"

#include "llvm/ADT/DenseMap.h"
#include "llvm/CallGraphSCCPass.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

/* Header file global to this project */
#include "CFCSS.h"

using namespace llvm;

namespace {
  class AssignFunctionSignatures : public CallGraphSCCPass {
    public:
      static char ID;

      AssignFunctionSignatures() : CallGraphSCCPass(ID) {
        functionSignatures = new DenseMap<Function*, unsigned long>(); 
        signatureUpdates = new DenseMap<Function*, std::pair<unsigned long, bool> >();
        nextSignature = 0;
      }

      virtual bool runOnSCC(CallGraphSCC &SCC) {
        for (CallGraphSCC::iterator node_iter = SCC.begin(), node_end = SCC.end();
            node_iter != node_end; ++node_iter) {

          CallGraphNode *node = *node_iter;

          if (Function *f = node->getFunction()) {
            DEBUG(errs() << "Assigning signature to " << f->getName().str() << ".\n");

            functionSignatures->insert(std::pair<Function*, unsigned long>(f, nextSignature++));

            for (CallGraphNode::iterator callee_iter = node->begin(), callee_end = node->end();
                callee_iter != callee_end; ++callee_iter) {

              CallGraphNode *callee = callee_iter->second;

              if (Function *called = callee->getFunction()) {
                DEBUG(errs() << "Generating signature update for call from "
                    << f->getName().str() << " to " << called->getName().str() << ".\n");

                unsigned long callee_signature = getSignature(called);

                if (std::pair<unsigned long, bool> *update = getSignatureUpdate(called)) {
                  DEBUG(errs() << "Signature update already exists.\n");
                } else {
                  unsigned long signatureUpdate = getSignature(f) ^ callee_signature;
                  DEBUG(errs() << "Signature update for " << called->getName().str()
                      << ": " << signatureUpdate << "\n");

                  bool multipleCallers = (callee->getNumReferences() > 1);

                  signatureUpdates->insert(std::pair<Function*,
                      std::pair<unsigned long, bool> >(called,
                        std::pair<unsigned long, bool>(signatureUpdate, multipleCallers)));
                }
              }
            }
          }
        }

        return false;
      }

      virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        // Documentation calls for this.
        CallGraphSCCPass::getAnalysisUsage(AU);

        AU.setPreservesCFG();
      }

      unsigned long getSignature(Function *f) {
        DenseMap<Function*, unsigned long>::iterator i = functionSignatures->find(f);

        return i->second;
      }

      std::pair<unsigned long, bool> *getSignatureUpdate(Function *f) {
        DenseMap<Function*, std::pair<unsigned long, bool> >::iterator i
            = signatureUpdates->find(f);

        if (i != signatureUpdates->end()) {

          return &(i->second);
        } else {

          return NULL;
        }
      }

    private:
      unsigned long nextSignature;
      DenseMap<Function*, unsigned long> *functionSignatures;
      DenseMap<Function*, std::pair<unsigned long, bool> > *signatureUpdates;
  };

  class InstrumentFunctions : public CallGraphSCCPass {
    public:
      static char ID;

      InstrumentFunctions() : CallGraphSCCPass(ID) {}

      virtual bool runOnSCC(CallGraphSCC &SCC) {
        AssignFunctionSignatures &AFS = getAnalysis<AssignFunctionSignatures>();

        for (CallGraphSCC::iterator i = SCC.begin(), e = SCC.end(); i < e; ++i) {
          if (Function *f = (*i)->getFunction()) {
            DEBUG(errs() << f->getName().str() << " has "
                << (*i)->getNumReferences() << " callers.\n");

            if ((*i)->getNumReferences() > 1) {
            } else {
            }
          }
        }

        return false;
      }

      virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        // Documentation calls for this.
        CallGraphSCCPass::getAnalysisUsage(AU);

        AU.setPreservesCFG();

        AU.addRequired<AssignFunctionSignatures>();
        AU.addPreserved<AssignFunctionSignatures>();
      }
  };
}

char AssignFunctionSignatures::ID = 0;

char InstrumentFunctions::ID = 0;

static RegisterPass<AssignFunctionSignatures> X("assignfsig", "AssignFunctionSignatures Pass", true, true);

static RegisterPass<InstrumentFunctions> Y("instrument-sigs", "InstrumentFunctions Pass", true, false);

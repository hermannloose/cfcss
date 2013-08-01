/**
 * @author Hermann Loose <hermannloose@gmail.com>
 *
 * TODO(hermannloose): Add description.
 */
#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"

namespace cfcss {

  typedef llvm::ConstantInt Signature;

  // TODO(hermannloose): Make size a template parameter.
  typedef llvm::SmallPtrSet<llvm::BasicBlock*, 64> BlockSet;
  typedef llvm::SmallPtrSet<llvm::Function*, 64> FunctionSet;

  typedef llvm::DenseMap<llvm::BasicBlock*, llvm::BasicBlock*> BlockToBlockMap;
  typedef llvm::DenseMap<llvm::BasicBlock*, llvm::Function*> BlockToFunctionMap;
  typedef llvm::DenseMap<llvm::BasicBlock*, Signature*> BlockToSignatureMap;

  typedef llvm::DenseMap<llvm::Function*, BlockSet*> FunctionToBlockSetMap;
  typedef llvm::DenseMap<llvm::Function*, llvm::Function*> FunctionToFunctionMap;
  typedef llvm::DenseMap<llvm::Function*, Signature*> FunctionToSignatureMap;

}

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

  typedef std::pair<llvm::BasicBlock*, llvm::BasicBlock*> BlockToBlockEntry;
  typedef std::pair<llvm::BasicBlock*, llvm::Function*> BlockToFunctionEntry;
  typedef std::pair<llvm::BasicBlock*, Signature*> BlockToSignatureEntry;

  typedef llvm::DenseMap<llvm::Function*, BlockSet*> FunctionToBlockSetMap;
  typedef llvm::DenseMap<llvm::Function*, llvm::Function*> FunctionToFunctionMap;

  typedef std::pair<llvm::Function*, llvm::Function*> FunctionToFunctionEntry;
}

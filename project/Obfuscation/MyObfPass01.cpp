//===- MyObfPass01.cpp - demo pass------------------------===//
//
// This file is just a demo for using BasicBlockPass.
//
//===----------------------------------------------------------------------===//
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
 
using namespace llvm;

namespace {
  struct MyObfPass01 : public BasicBlockPass {
      static char ID;
      MyObfPass01() : BasicBlockPass(ID) {}
 
      bool runOnBasicBlock(BasicBlock &BB) override {
        errs() << "I am running on a block. size: " << BB.size() << '\n';
        for(BasicBlock::iterator i = BB.begin(), e = BB.end(); i!=e; i++){
              errs() << *i << "\n";
        }
        return false;
      }
  };
}
 
char MyObfPass01::ID = 0;
static RegisterPass<MyObfPass01> X("myobfpass01", "Obfuscates zeroes tutorial 01");

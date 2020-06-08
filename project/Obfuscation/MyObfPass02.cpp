//===- MyObfPass02.cpp - demo pass------------------------===//
//
//  iterate over every instruction of the block and check if one of the operands is null.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Instructions.h"
 
using namespace llvm;
 
namespace {
 
  class MyObfPass02 : public BasicBlockPass {
 
    public:
    	static char ID;
    	MyObfPass02() : BasicBlockPass(ID) {}
    	bool runOnBasicBlock(BasicBlock &BB) override {
    		// Not iterating from the beginning to avoid obfuscation of Phi instructions
    		// parameters
    		for (typename BasicBlock::iterator I = BB.getFirstInsertionPt(), end = BB.end(); I != end; ++I) {
      			Instruction &Inst = *I;
      			for (size_t i = 0; i < Inst.getNumOperands(); ++i) {
          			if (Constant *C = isValidCandidateOperand(Inst.getOperand(i))) {
          				errs() << "I've found one sir! " << *C << '\n';
          			}
        		}
      		}
      		return false;
      	}
 
    private:
    	Constant *isValidCandidateOperand(Value *V) {
    		Constant *C;
    		if (!(C = dyn_cast<Constant>(V))) return nullptr;
    		if (!C->isNullValue()) return nullptr;
    		// We found a NULL constant, lets validate it
    		if(!C->getType()->isIntegerTy()) {
      		//dbgs() << "Ignoring non integer value\n";
      			return nullptr;
    		}
    		return C;
  	}
  };
}
 
char MyObfPass02::ID = 0;
static RegisterPass<MyObfPass02> X("myobfpass02", "Obfuscates zeroes tutorial 02");

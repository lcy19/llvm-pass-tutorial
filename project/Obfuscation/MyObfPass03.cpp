//===- MyObfPass03.cpp - demo pass------------------------===//
//
// register all the variables with integral type we come across while iterating through the block instructions
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include <vector>
 
using namespace llvm;
 
namespace {
 
  class MyObfPass03 : public BasicBlockPass {
    std::vector<Value *> IntegerVect;
 
    public:
    	static char ID;
    	MyObfPass03() : BasicBlockPass(ID) {}
    	bool runOnBasicBlock(BasicBlock &BB) override {
                IntegerVect.clear();
    		// Not iterating from the beginning to avoid obfuscation of Phi instructions parameters
    		for (typename BasicBlock::iterator I = BB.getFirstInsertionPt(), end = BB.end(); I != end; ++I) {
      			Instruction &Inst = *I;
      			for (size_t i = 0; i < Inst.getNumOperands(); ++i) {
          			if (Constant *C = isValidCandidateOperand(Inst.getOperand(i))) {
          				errs() << "I've found one sir!\n";
          			}
        		}
            		registerInteger(Inst);
      		}
      		return false;
      	}
 
    private:
      void registerInteger(Value &V) {
        if (V.getType()->isIntegerTy()) {
          IntegerVect.push_back(&V);
          errs() << "Registering an integer!" << V << "\n";
        }
      }
 
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
 
char MyObfPass03::ID = 0;
static RegisterPass<MyObfPass03> X("myobfpass03", "Obfuscates zeroes tutorial 03");

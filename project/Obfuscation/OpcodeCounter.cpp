//===- OpcodeCounter.cpp - Function pass------------------------===//
//
//  Count the number of operators in a function
//
//===----------------------------------------------------------------------===//
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
namespace{
	struct OpcodeCounter : public FunctionPass{
		static char ID;
		OpcodeCounter() : FunctionPass(ID){}

		bool runOnFunction(Function &F) override {
			std::map<std::string, int> opcodeCounter;
			errs() << "Function name: " << F.getName() << '\n'; 
			for(Function::iterator fun = F.begin(); fun != F.end(); fun++){
				for(BasicBlock::iterator bb = fun->begin(); bb != fun->end(); bb++){
					if(opcodeCounter.find(bb->getOpcodeName()) == opcodeCounter.end()){
						opcodeCounter[bb->getOpcodeName()] = 1;
					} else {
						opcodeCounter[bb->getOpcodeName()] += 1;
					}
				}
			}

			std::map<std::string, int>::iterator b = opcodeCounter.begin();
			std::map<std::string, int>::iterator e = opcodeCounter.end();
			while(b != e){
				llvm::outs() << b->first << " : " << b->second << '\n';
				b++;
			}
			llvm::outs() << '\n';
			opcodeCounter.clear();
			return false;
		}

	};
}

char OpcodeCounter::ID = 0;
static RegisterPass<OpcodeCounter> X("oc", "Opcode Counter", false, false);

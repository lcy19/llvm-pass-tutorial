//===- SimpleInvoker.cpp - Module pass------------------------===//
//
//  Check to see if the main function exists, create it if it doesn't exist
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"


using namespace llvm;
namespace{
    struct SimpleInvoker: public ModulePass{
        static char ID;
        SimpleInvoker() : ModulePass(ID){}
        bool runOnModule(Module &M) override;
    };
}
char SimpleInvoker::ID = 0;
bool SimpleInvoker::runOnModule(Module &M){
    Function *F = M.getFunction("main");
    if(F){
        errs() << "Main function found, so return \n";
        return true;
    }
    errs() << "Main function not found, so create\n";
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(M.getContext()), false);
    // 创建函数: 函数类型、连接类型、函数名、所在模块
    F = Function    ::Create(FT, GlobalValue::LinkageTypes::ExternalLinkage,"main", &M);
    // 为函数创建BasicBlock
    BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "EntryBlock", F);
    // 使用IRBuilder 快速完成指令插入
    IRBuilder<> IRB(EntryBB);
    for(Function &FF : M){
        if(FF.getName() == "main"){
            continue;
        }
        if(FF.empty()){
            continue;
        }
        IRB.CreateCall(&FF);
    }
    IRB.CreateRet(ConstantInt::get(Type::getInt32Ty(M.getContext()), 0));
    return true;
}
static RegisterPass<SimpleInvoker> X("si", "create main function");
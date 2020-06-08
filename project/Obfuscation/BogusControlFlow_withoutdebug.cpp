#include "BogusControlFlow.h"
#include "Utils.h"

// Stats
#define DEBUG_TYPE "BogusControlFlow"
STATISTIC(NumFunction, "a. Number of functions in this module");
STATISTIC(NumTimesOnFunctions, "b. Number of times we run on each function");
STATISTIC(InitNumBasicBlocks,
          "c. Initial number of basic blocks in this module");
STATISTIC(NumModifiedBasicBlocks, "d. Number of modified basic blocks");
STATISTIC(NumAddedBasicBlocks,
          "e. Number of added basic blocks in this module");
STATISTIC(FinalNumBasicBlocks,
          "f. Final number of basic blocks in this module");

using namespace llvm;

// Options for the pass
const int defaultObfRate = 30, defaultObfTime = 1;

static cl::opt<int>
    ObfProbRate("bcf_prob",
                cl::desc("Choose the probability [%] each basic blocks will be "
                         "obfuscated by the -bcf pass"),
                cl::value_desc("probability rate"), cl::init(defaultObfRate),
                cl::Optional);

static cl::opt<int>
    ObfTimes("bcf_loop",
             cl::desc("Choose how many time the -bcf pass loop on a function"),
             cl::value_desc("number of times"), cl::init(defaultObfTime),
             cl::Optional);

namespace {
struct BogusControlFlow : public FunctionPass {
  static char ID; // Pass identification
  bool flag;
  BogusControlFlow() : FunctionPass(ID) {}
  BogusControlFlow(bool flag) : FunctionPass(ID) {
    this->flag = flag;
    BogusControlFlow();
  }

  /* runOnFunction
   *
   * 重写FunctionPass方法
   * 每个basic block被混淆的几率
   * 它们的默认值分别为1和70%
   * 可通过设置参数boguscf-loop、 boguscf-prob修改它们的默认值
   * 入口函数
   */
  virtual bool runOnFunction(Function &F) {
    // 检查混效率是否合法
    if (ObfTimes <= 0) {
      errs() << "BogusControlFlow application number -bcf_loop=x must be x > 0";
      return false;
    }

    // 检查应用个数是否合法
    if (!((ObfProbRate > 0) && (ObfProbRate <= 100))) {
      errs() << "BogusControlFlow application basic blocks percentage "
                "-bcf_prob=x must be 0 < x <= 100";
      return false;
    }
    // 混淆函数
    bogus(F);
    // 
    doF(*F.getParent());
    return true;
  }

  void bogus(Function &F) {
    // 用于统计和调试
    ++NumFunction;
    int NumBasicBlocks = 0;
    bool firstTime = true; // 标记是否是第一次运行该程序
    bool hasBeenModified = false; //标记是否已经被修改过
    // 如果混淆率不合法
    if (ObfProbRate < 0 || ObfProbRate > 100) {
      ObfProbRate = defaultObfRate;
    }
    // 如果混淆次数不合法
    if (ObfTimes <= 0) {
      ObfTimes = defaultObfTime;
    }
    NumTimesOnFunctions = ObfTimes;
    int NumObfTimes = ObfTimes;

    // pass 的开始
    // 获取混淆的次数来控制循环
    do {
      // 将所有的代码块放入到list中
      std::list<BasicBlock *> basicBlocks;
      for (Function::iterator i = F.begin(); i != F.end(); ++i) {
        basicBlocks.push_back(&*i);
      }
      // 当基本块列表非空时
      while (!basicBlocks.empty()) {
        NumBasicBlocks++;
        // 选择基本块
        // 随机生成一个100以内的整数作为混淆率
        if ((int)llvm::cryptoutils->get_range(100) <= ObfProbRate) {
          hasBeenModified = true;
          ++NumModifiedBasicBlocks;
          NumAddedBasicBlocks += 3;
          FinalNumBasicBlocks += 3;
          // 循环调用addBogusFlow对选中的basicblock进行增加虚假控制流
          BasicBlock *basicBlock = basicBlocks.front();
          addBogusFlow(basicBlock, F);
        } else {
          DEBUG_WITH_TYPE("opt", errs() << "bcf: Block " << NumBasicBlocks
                                        << " not selected.\n");
        }
        // 每次处理完一个代码块后list弹出
        basicBlocks.pop_front();

        if (firstTime) { 
        // 第一次运行循环时初始化
          ++InitNumBasicBlocks;
          ++FinalNumBasicBlocks;
        }
      }
      // 改变flag
      firstTime = false;
    } while (--NumObfTimes > 0);
  }

  /* addBogusFlow
   * 对于给定的代码块 添加虚假控制流
   */
  virtual void addBogusFlow(BasicBlock *basicBlock, Function &F) {
    // 获取本basicblock中第一个不是Phi、debug、terminator的指令的地址
    BasicBlock::iterator i1 = basicBlock->begin();
    if (basicBlock->getFirstNonPHIOrDbgOrLifetime())
      i1 = (BasicBlock::iterator)basicBlock->getFirstNonPHIOrDbgOrLifetime();
    Twine *var;
    var = new Twine("originalBB");
    // 然后调用splitBasicBlock函数。该函数根据上述指令的地址将一个basicblock进行拆分
    BasicBlock *originalBB = basicBlock->splitBasicBlock(i1, *var);
    //对original basicblock进行拷贝生成一个名为altered basicblock的basicblock
    // 并对该basicblock加入一些垃圾指令
    Twine *var3 = new Twine("alteredBB");
    BasicBlock *alteredBB = createAlteredBasicBlock(originalBB, *var3, &F);
    // 至此所有的代码块都创建完毕，下面添加控制流
    // 清除first basicblock和altered basicblock跟父节点的关系，修改终结器以调整控制流程
    alteredBB->getTerminator()->eraseFromParent();
    basicBlock->getTerminator()->eraseFromParent();

    // 准备一条 1.0==1.0的恒真语句
    Value *LHS = ConstantFP::get(Type::getFloatTy(F.getContext()), 1.0);
    Value *RHS = ConstantFP::get(Type::getFloatTy(F.getContext()), 1.0);

    Twine *var4 = new Twine("condition");
    FCmpInst *condition =
        new FCmpInst(*basicBlock, FCmpInst::FCMP_TRUE, LHS, RHS, *var4);

    BranchInst::Create(originalBB, alteredBB, (Value *)condition, basicBlock);

    // 在altered模块的尾部增加一条跳转指令，当它执行完毕之后跳转到original basicblock模块(实际上它并不会执行)
    BranchInst::Create(originalBB, alteredBB);

    // 迭代器定位到最后一个originalBasicBlock
    BasicBlock::iterator i = originalBB->end();

    // Split at this point (we only want the terminator in the second part)
    Twine *var5 = new Twine("originalBBpart2");
    // 分割成两块
    BasicBlock *originalBBpart2 = originalBB->splitBasicBlock(--i, *var5);
    // 首先清除first basicblock和altered basicblock跟父节点的关系
    originalBB->getTerminator()->eraseFromParent();
    // We add at the end a new always true condition
    Twine *var6 = new Twine("condition2");
    // 将basicblock与alterbasicblock加到判断语句 1.0==1.0 的两侧
    FCmpInst *condition2 =
        new FCmpInst(*originalBB, CmpInst::FCMP_TRUE, LHS, RHS, *var6);
    BranchInst::Create(originalBBpart2, alteredBB, (Value *)condition2,
                       originalBB);
  } // end of addBogusFlow()

  /* createAlteredBasicBlock
   * 创建虚假指令块的核心方法
   * 此函数返回与给定块相似的基本块
   * 它被插入到给定的基本块之后指令相似
   * 但在之间添加了垃圾指令
   * 克隆指令的phi节点，元数据，用法和
   * 调试位置已调整为适合克隆的基本块
   */
  virtual BasicBlock *createAlteredBasicBlock(BasicBlock *basicBlock,
                                              const Twine &Name = "gen",
                                              Function *F = 0) {
    // 重映射操作数
    ValueToValueMapTy VMap;
    BasicBlock *alteredBB = llvm::CloneBasicBlock(basicBlock, VMap, Name, F);
    BasicBlock::iterator ji = basicBlock->begin();
    for (BasicBlock::iterator i = alteredBB->begin(), e = alteredBB->end();
         i != e; ++i) {
      // 循环执行指令的操作数
      for (User::op_iterator opi = i->op_begin(), ope = i->op_end(); opi != ope;
           ++opi) {
        // 获取操作数的值
        // 该方法返回函数局部值（Argument，Instruction，BasicBlock）的映射值，或计算并存储常数的值
        Value *v = MapValue(*opi, VMap, RF_None, 0);
        if (v != 0) {
          *opi = v;
        }
      }
      // Remap phi nodes' incoming blocks.
      if (PHINode *pn = dyn_cast<PHINode>(i)) {
        for (unsigned j = 0, e = pn->getNumIncomingValues(); j != e; ++j) {
          Value *v = MapValue(pn->getIncomingBlock(j), VMap, RF_None, 0);
          if (v != 0) {
            pn->setIncomingBlock(j, cast<BasicBlock>(v));
          }
        }
      }
      // Remap attached metadata.
      SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
      i->getAllMetadata(MDs);
      // important for compiling with DWARF, using option -g.
      i->setDebugLoc(ji->getDebugLoc());
      ji++;
    } // The instructions' informations are now all correct
    // 在块的中间添加随机指令
    for (BasicBlock::iterator i = alteredBB->begin(), e = alteredBB->end();
         i != e; ++i) {
      // 在找到二进制运算符的情况下，我们通过随机插入一些指令来对此部分进行稍微修改
      if (i->isBinaryOp()) { // binary instructions
        unsigned opcode = i->getOpcode();
        BinaryOperator *op, *op1 = NULL;
        Twine *var = new Twine("_");
        // 注意区别int float
        // 当操作是整形运算时
        if (opcode == Instruction::Add || opcode == Instruction::Sub ||
            opcode == Instruction::Mul || opcode == Instruction::UDiv ||
            opcode == Instruction::SDiv || opcode == Instruction::URem ||
            opcode == Instruction::SRem || opcode == Instruction::Shl ||
            opcode == Instruction::LShr || opcode == Instruction::AShr ||
            opcode == Instruction::And || opcode == Instruction::Or ||
            opcode == Instruction::Xor) {
          for (int random = (int)llvm::cryptoutils->get_range(10); random < 10;
               ++random) {
            switch (llvm::cryptoutils->get_range(4)) { // to improve
            case 0:                                    // do nothing
              break;
            case 1:
              op = BinaryOperator::CreateNeg(i->getOperand(0), *var, &*i);
              op1 = BinaryOperator::Create(Instruction::Add, op,
                                           i->getOperand(1), "gen", &*i);
              break;
            case 2:
              op1 = BinaryOperator::Create(Instruction::Sub, i->getOperand(0),
                                           i->getOperand(1), *var, &*i);
              op = BinaryOperator::Create(Instruction::Mul, op1,
                                          i->getOperand(1), "gen", &*i);
              break;
            case 3:
              op = BinaryOperator::Create(Instruction::Shl, i->getOperand(0),
                                          i->getOperand(1), *var, &*i);
              break;
            }
          }
        }
        // 当操作是浮点型运算时
        if (opcode == Instruction::FAdd || opcode == Instruction::FSub ||
            opcode == Instruction::FMul || opcode == Instruction::FDiv ||
            opcode == Instruction::FRem) {
          for (int random = (int)llvm::cryptoutils->get_range(10); random < 10;
               ++random) {
            switch (llvm::cryptoutils->get_range(3)) { // can be improved
            case 0:                                    // do nothing
              break;
            case 1:
              op = BinaryOperator::CreateFNeg(i->getOperand(0), *var, &*i);
              op1 = BinaryOperator::Create(Instruction::FAdd, op,
                                           i->getOperand(1), "gen", &*i);
              break;
            case 2:
              op = BinaryOperator::Create(Instruction::FSub, i->getOperand(0),
                                          i->getOperand(1), *var, &*i);
              op1 = BinaryOperator::Create(Instruction::FMul, op,
                                           i->getOperand(1), "gen", &*i);
              break;
            }
          }
        }
        // 当操作是整形比较时
        if (opcode == Instruction::ICmp) { // Condition (with int)
          ICmpInst *currentI = (ICmpInst *)(&i);
          switch (llvm::cryptoutils->get_range(3)) { // must be improved
          case 0:                                    // do nothing
            break;
          case 1:
            currentI->swapOperands();
            break;
          case 2: // randomly change the predicate
            switch (llvm::cryptoutils->get_range(10)) {
            case 0:
              currentI->setPredicate(ICmpInst::ICMP_EQ);
              break; // equal
            case 1:
              currentI->setPredicate(ICmpInst::ICMP_NE);
              break; // not equal
            case 2:
              currentI->setPredicate(ICmpInst::ICMP_UGT);
              break; // unsigned greater than
            case 3:
              currentI->setPredicate(ICmpInst::ICMP_UGE);
              break; // unsigned greater or equal
            case 4:
              currentI->setPredicate(ICmpInst::ICMP_ULT);
              break; // unsigned less than
            case 5:
              currentI->setPredicate(ICmpInst::ICMP_ULE);
              break; // unsigned less or equal
            case 6:
              currentI->setPredicate(ICmpInst::ICMP_SGT);
              break; // signed greater than
            case 7:
              currentI->setPredicate(ICmpInst::ICMP_SGE);
              break; // signed greater or equal
            case 8:
              currentI->setPredicate(ICmpInst::ICMP_SLT);
              break; // signed less than
            case 9:
              currentI->setPredicate(ICmpInst::ICMP_SLE);
              break; // signed less or equal
            }
            break;
          }
        }
        // 当操作是浮点型比较时
        if (opcode == Instruction::FCmp) { // Conditions (with float)
          FCmpInst *currentI = (FCmpInst *)(&i);
          switch (llvm::cryptoutils->get_range(3)) { // must be improved
          case 0:                                    // do nothing
            break;
          case 1:
            currentI->swapOperands();
            break;
          case 2: // randomly change the predicate
            switch (llvm::cryptoutils->get_range(10)) {
            case 0:
              currentI->setPredicate(FCmpInst::FCMP_OEQ);
              break; // ordered and equal
            case 1:
              currentI->setPredicate(FCmpInst::FCMP_ONE);
              break; // ordered and operands are unequal
            case 2:
              currentI->setPredicate(FCmpInst::FCMP_UGT);
              break; // unordered or greater than
            case 3:
              currentI->setPredicate(FCmpInst::FCMP_UGE);
              break; // unordered, or greater than, or equal
            case 4:
              currentI->setPredicate(FCmpInst::FCMP_ULT);
              break; // unordered or less than
            case 5:
              currentI->setPredicate(FCmpInst::FCMP_ULE);
              break; // unordered, or less than, or equal
            case 6:
              currentI->setPredicate(FCmpInst::FCMP_OGT);
              break; // ordered and greater than
            case 7:
              currentI->setPredicate(FCmpInst::FCMP_OGE);
              break; // ordered and greater than or equal
            case 8:
              currentI->setPredicate(FCmpInst::FCMP_OLT);
              break; // ordered and less than
            case 9:
              currentI->setPredicate(FCmpInst::FCMP_OLE);
              break; // ordered or less than, or equal
            }
            break;
          }
        }
      }
    }
    return alteredBB;
  } // end of createAlteredBasicBlock()

  /*
  * 替换恒真谓词函数
  * 产生随机化的恒真谓词
  */
  void replaceTrueOp (Module &M,std::vector<Instruction *> toEdit,GlobalVariable *x,GlobalVariable *y){
  	BinaryOperator *op, *op1 = NULL;
    LoadInst *opX, *opY;
    ICmpInst *condition, *condition2;
    int random = 0;
    // 编列需要替换的位置
  	for (std::vector<Instruction *>::iterator i = toEdit.begin();
         i != toEdit.end(); ++i) {
      // 获取两个操作变量 X Y
      opX = new LoadInst((Value *)x, "", (*i));
      opY = new LoadInst((Value *)y, "", (*i));
      // 获取一个随机数
      random = (int)llvm::cryptoutils->get_range(125);
      // 通过随机数的不同取值进行不同的混淆
      if (random % 4 == 0){
        op = BinaryOperator::Create(
          Instruction::Add, (Value *)opX,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 1, false), "",
          (*i));
      op1 =
          BinaryOperator::Create(Instruction::Mul, (Value *)opX, op, "", (*i));
      op = BinaryOperator::Create(
          Instruction::URem, op1,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 2, false), "",
          (*i));
      condition = new ICmpInst(
          (*i), ICmpInst::ICMP_EQ, op,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false));
      condition2 = new ICmpInst(
          (*i), ICmpInst::ICMP_SLT, opY,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), (int)llvm::cryptoutils->get_range(125)+1, false));
      op1 = BinaryOperator::Create(Instruction::Or, (Value *)condition,
                                   (Value *)condition2, "", (*i));
      }
      else if (random % 4 == 1){
        op = BinaryOperator::Create(
          Instruction::Add, (Value *)opX,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 3, false), "",
          (*i));
      op1 =
          BinaryOperator::Create(Instruction::Mul,op, (Value *)opX, "", (*i));
      op = BinaryOperator::Create(
          Instruction::URem, op1,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 2, false), "",
          (*i));
      condition = new ICmpInst(
          (*i), ICmpInst::ICMP_EQ, op,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false));
      condition2 = new ICmpInst(
          (*i), ICmpInst::ICMP_SLT, opY,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), (int)llvm::cryptoutils->get_range(125)+1, false));
      op1 = BinaryOperator::Create(Instruction::Or, (Value *)condition2,
                                   (Value *)condition, "", (*i));
      }
      else if (random % 4 == 2) {
        op = BinaryOperator::Create(
          Instruction::Add, (Value *)opX,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 5, false), "",
          (*i));
      op1 =
          BinaryOperator::Create(Instruction::Mul, op, (Value *)opX,"", (*i));
      op = BinaryOperator::Create(
          Instruction::URem, op1,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 2, false), "",
          (*i));
      condition = new ICmpInst(
          (*i), ICmpInst::ICMP_EQ, op,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false));
      condition2 = new ICmpInst(
          (*i), ICmpInst::ICMP_SLT, opY,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), (int)llvm::cryptoutils->get_range(125)+1, false));
      op1 = BinaryOperator::Create(Instruction::Or, (Value *)condition2,
                                   (Value *)condition, "", (*i));
      }
      else {
        op = BinaryOperator::Create(
          Instruction::Add, (Value *)opX,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 7, false), "",
          (*i));
      op1 =
          BinaryOperator::Create(Instruction::Mul, op, (Value *)opX,"", (*i));
      op = BinaryOperator::Create(
          Instruction::URem, op1,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 2, false), "",
          (*i));
      condition = new ICmpInst(
          (*i), ICmpInst::ICMP_EQ, op,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false));
      condition2 = new ICmpInst(
          (*i), ICmpInst::ICMP_SLT, opY,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), (int)llvm::cryptoutils->get_range(125)+1, false));
      op1 = BinaryOperator::Create(Instruction::Or, (Value *)condition2,
                                   (Value *)condition, "", (*i));
      }
      BranchInst::Create(((BranchInst *)*i)->getSuccessor(0),
                         ((BranchInst *)*i)->getSuccessor(1), (Value *)op1,
                         ((BranchInst *)*i)->getParent());
      (*i)->eraseFromParent(); // erase the branch
    }
    return;
  	
  }

  /* doFinalization
   *
   * 混淆所有恒真谓词
   */
  bool doF(Module &M) {
    // 创建两个全局变量 x y
    Twine *varX = new Twine("x");
    Twine *varY = new Twine("y");
    // 将 x y初始化为0
    Value *x1 = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false);
    Value *y1 = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false);

    GlobalVariable *x =
        new GlobalVariable(M, Type::getInt32Ty(M.getContext()), false,
                           GlobalValue::CommonLinkage, (Constant *)x1, *varX);
    GlobalVariable *y =
        new GlobalVariable(M, Type::getInt32Ty(M.getContext()), false,
                           GlobalValue::CommonLinkage, (Constant *)y1, *varY);

    std::vector<Instruction *> toEdit, toDelete;
    BinaryOperator *op, *op1 = NULL;
    LoadInst *opX, *opY;
    ICmpInst *condition, *condition2;
    // 寻找需要替换的恒真谓词
    for (Module::iterator mi = M.begin(), me = M.end(); mi != me; ++mi) {
      for (Function::iterator fi = mi->begin(), fe = mi->end(); fi != fe;
           ++fi) {
        // fi->setName("");
        Instruction *tbb = fi->getTerminator();
        if (tbb->getOpcode() == Instruction::Br) {
          BranchInst *br = (BranchInst *)(tbb);
          if (br->isConditional()) {
            FCmpInst *cond = (FCmpInst *)br->getCondition();
            unsigned opcode = cond->getOpcode();
            if (opcode == Instruction::FCmp) {
              if (cond->getPredicate() == FCmpInst::FCMP_TRUE) {
                // 标记需要替换的代码位置
                toDelete.push_back(cond);
                toEdit.push_back(tbb);
              }
            }
          }
        }
      }
    }
    // 对指令进行替换
    replaceTrueOp(M,toEdit,x,y);
    // 删除原指令
    for (std::vector<Instruction *>::iterator i = toDelete.begin();
         i != toDelete.end(); ++i) {
      (*i)->eraseFromParent();
    }
    for (Module::iterator mi = M.begin(), me = M.end(); mi != me; ++mi) {
      DEBUG_WITH_TYPE("cfg", errs()
                                 << "bcf: Function " << mi->getName() << "\n");
      DEBUG_WITH_TYPE("cfg", mi->viewCFG());
    }
    return true;
  }
};
}

char BogusControlFlow::ID = 0;
static RegisterPass<BogusControlFlow> X("boguscf",
                                        "inserting bogus control flow");

Pass *llvm::createBogus() { return new BogusControlFlow(); }

Pass *llvm::createBogus(bool flag) { return new BogusControlFlow(flag); }

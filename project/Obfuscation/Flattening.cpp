//===- Flattening.cpp - Flattening Obfuscation pass------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the flattening pass
//
//===----------------------------------------------------------------------===//

#include "Flattening.h"
#include "Utils.h"
#include "llvm/Transforms/Scalar.h"
#include "CryptoUtils.h"

#define DEBUG_TYPE "flattening"

using namespace llvm;

// Stats
STATISTIC(Flattened, "Functions flattened");

namespace {
    struct Flattening : public FunctionPass {
        static char ID;  // Pass identification, replacement for typeid
        bool flag{};

        Flattening() : FunctionPass(ID) {}
        explicit Flattening(bool flag) : FunctionPass(ID) { this->flag = flag; }

        bool runOnFunction(Function &F) override;
        static bool flatten(Function *f);
    };
}

char Flattening::ID = 0;
static RegisterPass<Flattening> X("flattening", "Call graph flattening");
Pass *llvm::createFlattening(bool flag) { return new Flattening(flag); }

bool Flattening::runOnFunction(Function &F) {
    Function *tmp = &F;
    // Do we obfuscate

    if (flatten(tmp)) {
        ++Flattened;
    }

    return false;
}

bool Flattening::flatten(Function *f) {
    std::vector<BasicBlock *> origBB;
    BasicBlock *loopEntry;
    BasicBlock *loopEnd;
    LoadInst *load;
    SwitchInst *switchI;
    AllocaInst *switchVar;

    // SCRAMBLER
    char scrambling_key[16];
    llvm::cryptoutils->get_bytes(scrambling_key, 16);
    // END OF SCRAMBLER

    // Lower switch
#if LLVM_VERSION_MAJOR >= 9
    // >=9.0, LowerSwitchPass depends on LazyValueInfoWrapperPass, which cause AssertError.
    // So I move LowerSwitchPass into register function, just before FlatteningPass.
#else
    FunctionPass *lower = createLowerSwitchPass();
    lower->runOnFunction(*f);
#endif

    // Save all original BB
    for (Function::iterator i = f->begin(); i != f->end(); ++i) {
        BasicBlock *tmp = &*i;
        origBB.push_back(tmp);

        BasicBlock *bb = &*i;
        if (isa<InvokeInst>(bb->getTerminator())) {
            return false;
        }
    }

    // Nothing to flatten
    if (origBB.size() <= 1) {
        return false;
    }

    // Remove first BB
    origBB.erase(origBB.begin());

    // Get a pointer on the first BB
    Function::iterator tmp = f->begin();  //++tmp;
    BasicBlock *insert = &*tmp;

    // If main begin with an if
    BranchInst *br = nullptr;
    if (isa<BranchInst>(insert->getTerminator())) {
        br = cast<BranchInst>(insert->getTerminator());
    }

    if ((br != nullptr && br->isConditional()) ||
        insert->getTerminator()->getNumSuccessors() > 1) {
        BasicBlock::iterator i = insert->end();
        --i;

        if (insert->size() > 1) {
            --i;
        }

        BasicBlock *tmpBB = insert->splitBasicBlock(i, "first");
        origBB.insert(origBB.begin(), tmpBB);
    }

    // Remove jump
    insert->getTerminator()->eraseFromParent();

    // Create switch variable and set as it
    switchVar =
            new AllocaInst(Type::getInt32Ty(f->getContext()), 0, "switchVar", insert);
    new StoreInst(
            ConstantInt::get(Type::getInt32Ty(f->getContext()),
                             llvm::cryptoutils->scramble32(0, scrambling_key)),
            switchVar, insert);

    // Create main loop
    loopEntry = BasicBlock::Create(f->getContext(), "loopEntry", f, insert);
    loopEnd = BasicBlock::Create(f->getContext(), "loopEnd", f, insert);

    load = new LoadInst(switchVar, "switchVar", loopEntry);

    // Move first BB on top
    insert->moveBefore(loopEntry);
    BranchInst::Create(loopEntry, insert);

    // loopEnd jump to loopEntry
    BranchInst::Create(loopEntry, loopEnd);

    BasicBlock *swDefault =
            BasicBlock::Create(f->getContext(), "switchDefault", f, loopEnd);
    BranchInst::Create(loopEnd, swDefault);

    // Create switch instruction itself and set condition
    switchI = SwitchInst::Create(&*f->begin(), swDefault, 0, loopEntry);
    switchI->setCondition(load);

    // Remove branch jump from 1st BB and make a jump to the while
    f->begin()->getTerminator()->eraseFromParent();

    BranchInst::Create(loopEntry, &*f->begin());

    // Put all BB in the switch
    for (std::vector<BasicBlock *>::iterator b = origBB.begin(); b != origBB.end();
         ++b) {
        BasicBlock *i = *b;
        ConstantInt *numCase = nullptr;

        // Move the BB inside the switch (only visual, no code logic)
        i->moveBefore(loopEnd);

        // Add case to switch
        numCase = cast<ConstantInt>(ConstantInt::get(
                switchI->getCondition()->getType(),
                llvm::cryptoutils->scramble32(switchI->getNumCases(), scrambling_key)));
        switchI->addCase(numCase, i);
    }

    // Recalculate switchVar
    for (std::vector<BasicBlock *>::iterator b = origBB.begin(); b != origBB.end();
         ++b) {
        BasicBlock *i = *b;
        ConstantInt *numCase = nullptr;

        // Ret BB
        if (i->getTerminator()->getNumSuccessors() == 0) {
            continue;
        }

        // If it's a non-conditional jump
        if (i->getTerminator()->getNumSuccessors() == 1) {
            // Get successor and delete terminator
            BasicBlock *succ = i->getTerminator()->getSuccessor(0);
            i->getTerminator()->eraseFromParent();

            // Get next case
            numCase = switchI->findCaseDest(succ);

            // If next case == default case (switchDefault)
            if (numCase == nullptr) {
                numCase = cast<ConstantInt>(
                        ConstantInt::get(switchI->getCondition()->getType(),
                                         llvm::cryptoutils->scramble32(
                                                 switchI->getNumCases() - 1, scrambling_key)));
            }

            // Update switchVar and jump to the end of loop
            new StoreInst(numCase, load->getPointerOperand(), i);
            BranchInst::Create(loopEnd, i);
            continue;
        }

        // If it's a conditional jump
        if (i->getTerminator()->getNumSuccessors() == 2) {
            // Get next cases
            ConstantInt *numCaseTrue =
                    switchI->findCaseDest(i->getTerminator()->getSuccessor(0));
            ConstantInt *numCaseFalse =
                    switchI->findCaseDest(i->getTerminator()->getSuccessor(1));

            // Check if next case == default case (switchDefault)
            if (numCaseTrue == nullptr) {
                numCaseTrue = cast<ConstantInt>(
                        ConstantInt::get(switchI->getCondition()->getType(),
                                         llvm::cryptoutils->scramble32(
                                                 switchI->getNumCases() - 1, scrambling_key)));
            }

            if (numCaseFalse == nullptr) {
                numCaseFalse = cast<ConstantInt>(
                        ConstantInt::get(switchI->getCondition()->getType(),
                                         llvm::cryptoutils->scramble32(
                                                 switchI->getNumCases() - 1, scrambling_key)));
            }

            // Create a SelectInst
            BranchInst *br = cast<BranchInst>(i->getTerminator());
            SelectInst *sel =
                    SelectInst::Create(br->getCondition(), numCaseTrue, numCaseFalse, "",
                                       i->getTerminator());

            // Erase terminator
            i->getTerminator()->eraseFromParent();

            // Update switchVar and jump to the end of loop
            new StoreInst(sel, load->getPointerOperand(), i);
            BranchInst::Create(loopEnd, i);
            continue;
        }
    }

    fixStack(f);

    return true;
}
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/Cloning.h" // [Transformation]
#include "llvm/ADT/DenseMap.h" // [Transformation]
#include "llvm/ADT/SmallVector.h" // [Transformation]
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include  <iostream>
#include  <unordered_set>
#include  <queue>

using namespace llvm;

#define THRESHOLD 0.0 // [0.0,1.0]
#define SUBLOOPTHRESHOLD 1.0  // [0.0,1.0]
#define SELECTIVE_RATE 4  // for now, only support power of 2
#define FLIP_PERF false
// O: original code
// P: perforated code
// FLIP_PERF = false: O O O P O O O P ...
// FLIP_PERF = true:  P P P O P P P O ...

namespace {

  struct CountFrequencyPass : public PassInfoMixin<CountFrequencyPass> {
    static uint64_t total_freq;

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
      llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);

      for (BasicBlock &BB : F) {
        uint64_t bb_freq = bfi.getBlockFreq(&BB).getFrequency();
        uint64_t num_instr = std::distance(BB.begin(), BB.end());
        total_freq += bb_freq * num_instr;
      }

      return PreservedAnalyses::all();
    }
  };

  struct LoopPerforationPass : public PassInfoMixin<LoopPerforationPass> {

    typedef struct LoopInfoT {
          Loop* L;
          uint64_t instr_count;
    } LoopInfo;

    void FindInnerMost(std::vector<Loop*>& loop_vec, Loop* L) {
      if (L->isInnermost()) {
        loop_vec.emplace_back(L);
        return;
      }
      for (Loop *SL : L->getSubLoops()) {
        FindInnerMost(loop_vec, SL);
      }
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
      llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
      llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);

      // Find the loops that have a lot of iterations and instructions
      std::vector<LoopInfo> loop_info;

      for (auto &loop : li.getTopLevelLoopsVector()) {
        // skip unsimplified loops
        if (!loop->isLoopSimplifyForm()) {
          continue;
        }

        // skip loops that don't use an induction variable
        // if (loop->getCanonicalInductionVariable() == nullptr) {
        //   continue;
        // }

        uint64_t instr_count = 0;
        for (auto &bb : loop->getBlocksVector()) {
          uint64_t bb_freq = bfi.getBlockFreq(bb).getFrequency();
          uint64_t num_instr = std::distance(bb->begin(), bb->end());
          instr_count += bb_freq * num_instr;
        }
        
        bool perf_sub = false;
        std::vector<Loop*> inner_most;
        Loop* cur_loop = loop;
        FindInnerMost(inner_most, loop);
        for (Loop* sub_loop : inner_most) {
          uint64_t sub_instr_count = 0;
          for (auto &bb : sub_loop->getBlocksVector()) {
            uint64_t bb_freq = bfi.getBlockFreq(bb).getFrequency();
            uint64_t num_instr = std::distance(bb->begin(), bb->end());
            sub_instr_count += bb_freq * num_instr;
          }
          if ((double)sub_instr_count > SUBLOOPTHRESHOLD * (double)instr_count) {
            loop_info.emplace_back(LoopInfo{sub_loop, sub_instr_count});
            perf_sub = true;
          }
        }

        if (!perf_sub)
          loop_info.emplace_back(LoopInfo{loop, instr_count});
      }

      std::vector<Loop*> loop_candidates;

      // The loops whose total frequency exceeds the threshold are selected as candidates
      for (auto &loop : loop_info) {
        if ((double)loop.instr_count / (double)CountFrequencyPass::total_freq > THRESHOLD){
          loop_candidates.emplace_back(loop.L);
          // errs() << format("%d / %d\n", loop.instr_count, CountFrequencyPass::total_freq);  
        }
      }

      // errs() << format("Loop candidates: %d\n", loop_candidates.size());

      /** Selective Perforation **/

      for (Loop* loop : loop_candidates) {
        std::unordered_set<Instruction*> all_instr; // all instructions in the loop
        for (BasicBlock *BB : loop->getBlocks()) {
          for (Instruction &I : *BB) {
            all_instr.emplace(&I);
          }
        }

        /** SELECT BEGINS **/
        // Mark those instructions that cannot be perforated
        std::queue<Instruction*> exclude_queue;
        std::unordered_set<Instruction*> exclude_set;
        for (BasicBlock *BB : loop->getBlocks()) {
          for (Instruction &I : *BB) {
            unsigned op = I.getOpcode();
            // exclude branches
            if (op == Instruction::Br ||
                op == Instruction::Switch ||
                op == Instruction::IndirectBr ||
                op == Instruction::CallBr ||
                op == Instruction::Call) {
              exclude_queue.push(&I);
              exclude_set.emplace(&I);
            } 
            // exclude pointer calculations
            else if (I.getType()->isPointerTy()) {
              exclude_queue.push(&I);
              exclude_set.emplace(&I);
            }
            
            // else {
            //   // operands of instructions: https://llvm.org/docs/LangRef.html
            //   Instruction* ptr_instr;
            //   if (op == Instruction::Load){
            //     ptr_instr = dyn_cast<llvm::Instruction>(I.getOperand(0));
            //   } else if (op == Instruction::Store) {
            //     ptr_instr = dyn_cast<llvm::Instruction>(I.getOperand(1));
            //   } else if (op == Instruction::GetElementPtr) {
            //     ptr_instr = dyn_cast<llvm::Instruction>(I.getOperand(1));
            //   } else if (op == Instruction::PtrToInt) {
            //     ptr_instr = dyn_cast<llvm::Instruction>(I.getOperand(0));
            //   } else {
            //     continue;
            //   }

            //   if (all_instr.find(ptr_instr) != all_instr.end() &&
            //       exclude_set.find(ptr_instr) == exclude_set.end()) {
            //     exclude_queue.push(ptr_instr);
            //     exclude_set.emplace(ptr_instr);
            //   }
            // }
          }
        }

        // exclude loop inductions
        PHINode* canonical = loop->getCanonicalInductionVariable();
        if (canonical != NULL) {
          if (exclude_set.find(canonical) == exclude_set.end()) {
            exclude_queue.push(canonical);
            exclude_set.emplace(canonical);
          }
        }

        while (!exclude_queue.empty()) {
          Instruction* cur_instr = exclude_queue.front();
          exclude_queue.pop();
          for (int i = 0; i < cur_instr->getNumOperands(); ++i) {
            Value* source_value = cur_instr->getOperand(i);
            Instruction* source_instr = dyn_cast<llvm::Instruction>(source_value);
            if (all_instr.find(source_instr) == all_instr.end())
              continue;
            if (exclude_set.find(source_instr) == exclude_set.end()) {
              exclude_queue.push(source_instr);
              exclude_set.emplace(source_instr);
            }
          }

          // exclude induction variables in the load/store pair form
          if (cur_instr->getOpcode() == Instruction::Load) {
            bool isa_induct = false;
            for (User* U:cur_instr->users()){ // U is of type User *
              if (Instruction* I = dyn_cast<Instruction>(U)){
                unsigned use_op = I->getOpcode();
                if (all_instr.find(I) != all_instr.end()) {
                  if (use_op == Instruction::ICmp) {
                    isa_induct = true;
                    break;
                  }
                }
              }
            }
            if (isa_induct) {
              Value* source_ptr = cur_instr->getOperand(0);
              Instruction* source_ptr_instr = dyn_cast<llvm::Instruction>(source_ptr);
              for (Instruction* find_store : all_instr) {
                if (find_store->getOpcode() == Instruction::Store && find_store->getOperand(1) == source_ptr) {
                  if (exclude_set.find(find_store) == exclude_set.end()) {
                    exclude_queue.push(find_store);
                    exclude_set.emplace(find_store);
                  }
                }
              }
            }
          }
        }

        // select stores and loads
        std::unordered_set<Instruction*> target_instr;
        std::queue<Instruction*> instr_queue;
        for (BasicBlock *BB : loop->getBlocks()) {
          for (Instruction &I : *BB) {
            if (I.getOpcode() == Instruction::Store && !I.getOperand(0)->getType()->isPointerTy() && exclude_set.find(&I) == exclude_set.end()) {
              target_instr.emplace(&I);
              instr_queue.push(&I);
            } else if (I.getOpcode() == Instruction::Load && exclude_set.find(&I) == exclude_set.end()) {
              target_instr.emplace(&I);
              instr_queue.push(&I);
            }
          }
        }

        // errs() << format("All instructions in this loop: %d\n", all_instr.size());
        /** END OF SELECT **/

        /** EXPANSION BEGINS **/
        while (!instr_queue.empty()){
          // errs() << format("%d\n", target_instr.size());
          Instruction* cur_instr = instr_queue.front();
          instr_queue.pop();

          // Check all the operand values
          for (int i = 0; i < cur_instr->getNumOperands(); ++i) {
            Value* source_value = cur_instr->getOperand(i);
            Instruction* source_instr = dyn_cast<llvm::Instruction>(source_value);

            // exclude the instructions not in the loop
            if (all_instr.find(source_instr) == all_instr.end())
              continue;

            // skip if the instruction is already targeted
            if (target_instr.find(source_instr) != target_instr.end())
              continue;

            // skip if the instruction is marked as not perforable
            if (exclude_set.find(source_instr) != exclude_set.end())
              continue;

            bool perforable = true;
            for (User* U:source_value->users()){ // U is of type User *
              if (Instruction* I = dyn_cast<Instruction>(U)){
                //an instruction uses the source_instr
                if (target_instr.find(I) == target_instr.end()) {
                  perforable = false;
                  break;
                }
              }
            }
            if (perforable) {
              target_instr.emplace(source_instr);
              instr_queue.push(source_instr);
            }
          }

          // Check all the uses
          for (User* U:cur_instr->users()){ // U is of type User *
            if (Instruction* dest_instr = dyn_cast<Instruction>(U)){
              //an instruction uses V
              // exclude the instructions not in the loop
              if (all_instr.find(dest_instr) == all_instr.end())
                continue;

              // skip if the instruction is already targeted
              if (target_instr.find(dest_instr) != target_instr.end())
                continue;

              // skip if the instruction is marked as not perforable
              if (exclude_set.find(dest_instr) != exclude_set.end())
                continue;
              
              bool perforable = true;
              for (int i = 0; i < dest_instr->getNumOperands(); ++i) {
                Value* dest_source_value = dest_instr->getOperand(i);
                Instruction* dest_source_instr = dyn_cast<llvm::Instruction>(dest_source_value);
                if (target_instr.find(dest_source_instr) == target_instr.end() &&
                    !loop->isLoopInvariant(dest_source_value)) {
                  perforable = false;
                  break;
                }
              }
              if (perforable) {
                target_instr.emplace(dest_instr);
                instr_queue.push(dest_instr);
              }
            }
          }
        }

        // errs() << format("All instructions in this loop: %d\n", all_instr.size());
        // errs() << format("Perforated instructions: %d\n", target_instr.size());

        /** END OF EXPANSION **/

        // At this point, all the perforatable instructions are in
        // std::unordered_set<Instruction*> target_instr

        /** TRANFORMATION BEGINS **/
        // If target_instr is empty, don't modify the loop
        if (target_instr.empty()) continue;
        
        // 1. Duplicate the loop body to create a perforated version
        SmallVector<BasicBlock*, 4> perforatedBlocks(loop->getBlocks().begin(), loop->getBlocks().end());
        SmallVector<BasicBlock*, 4> originalBlocksCopy;
        ValueToValueMapTy blockMap;

        for (BasicBlock* BB : perforatedBlocks) {
          BasicBlock* ClonedBB = CloneBasicBlock(BB, blockMap, ".perf", &F);
          originalBlocksCopy.push_back(ClonedBB);
          blockMap[BB] = ClonedBB;
        }

        // 2. Remapping Instructions
        for (BasicBlock* BB : originalBlocksCopy) {
          for (Instruction &I : *BB) {
            RemapInstruction(&I, blockMap, RF_NoModuleLevelChanges | RF_IgnoreMissingLocals);
          }
        }

        // 3. Modifying the Perforated Version
        std::vector<Instruction*> to_be_removed;
        for (BasicBlock* BB : perforatedBlocks) {
          int count = 0;
          for (Instruction &I : *BB) {
            // If is perforate target, remove it from the basic block
            if (target_instr.count(&I)) {
              to_be_removed.emplace_back(&I);
            }
          }
        }

        for (Instruction* remove_instr : to_be_removed) {
          auto *new_I = Constant::getNullValue(remove_instr->getType());
          remove_instr->replaceAllUsesWith(new_I);
          remove_instr->eraseFromParent();
        }

        // 4. Inserting Conditional Branch
        BasicBlock *oldHeader = loop->getHeader();
        BasicBlock *preHeader = loop->getLoopPreheader();
        // New entry point for loop
        BasicBlock *newHeader = BasicBlock::Create(F.getContext(), "loop_perforated_entry", &F, cast<BasicBlock>(blockMap[oldHeader]));
        IRBuilder<> builder(newHeader);
        // ##### What condition? #####
        Value *dummyCondition = builder.CreateICmpEQ(builder.getInt1(true), builder.getInt1(true), "dummyCondition");
        BranchInst *dummybranch = builder.CreateCondBr(dummyCondition, oldHeader, cast<BasicBlock>(blockMap[oldHeader]));
        for (BasicBlock *Pred : predecessors(oldHeader)) {
          if (Pred != newHeader)
            Pred->getTerminator()->replaceUsesOfWith(oldHeader, newHeader);
        }
        for (BasicBlock *Pred : predecessors(cast<BasicBlock>(blockMap[oldHeader]))) {
          if (Pred != newHeader)
            Pred->getTerminator()->replaceUsesOfWith(cast<BasicBlock>(blockMap[oldHeader]), newHeader);
        }
        preHeader->getTerminator()->replaceUsesOfWith(oldHeader, newHeader);

        static LLVMContext ctx;
        Instruction *induct_init = BinaryOperator::CreateAdd(ConstantInt::get(Type::getInt32Ty(ctx), 0), ConstantInt::get(Type::getInt32Ty(ctx), 0));
        induct_init->insertBefore(preHeader->getTerminator());

        PHINode* new_phi = llvm::PHINode::Create(Type::getInt32Ty(ctx), 0, "new_phi", &(newHeader->front()));

        new_phi->addIncoming(induct_init, preHeader);
        for (BasicBlock* p_bb : predecessors(newHeader)) {
          if (p_bb == preHeader) continue;
          Instruction *increment = BinaryOperator::CreateAdd(new_phi, ConstantInt::get(Type::getInt32Ty(ctx), 1));
          increment->insertBefore(p_bb->getTerminator());
          new_phi->addIncoming(increment, p_bb);
        }

        Value *mod = builder.CreateAnd(new_phi, ConstantInt::get(Type::getInt32Ty(ctx), SELECTIVE_RATE-1));
        // Value *mult = builder.CreateMul(udiv, ConstantInt::get(Type::getInt32Ty(ctx), SELECTIVE_RATE));
        // Value *diff = builder.CreateSub(new_phi, mult);

        Value *loopCondition = builder.CreateICmpEQ(mod, ConstantInt::get(Type::getInt32Ty(ctx), 0), "loopCondition");
        if (FLIP_PERF) {
          BranchInst *branch = builder.CreateCondBr(loopCondition, cast<BasicBlock>(blockMap[oldHeader]), oldHeader);
        } else {
          BranchInst *branch = builder.CreateCondBr(loopCondition, oldHeader, cast<BasicBlock>(blockMap[oldHeader]));
        }
        dummybranch->eraseFromParent();

        // 5. Modify loop exits
        // for (BasicBlock* BB : perforatedBlocks) {
        //   Instruction *term = BB->getTerminator();
        //   for (unsigned i = 0, nsucc = term->getNumSuccessors(); i < nsucc; ++i) {
        //     BasicBlock *succ = term->getSuccessor(i);
        //     // Check if successor is Within the Loop
        //     /* If it is, this iteration of the inner loop is skipped. 
        //     We are only interested in successors that reprsent exits 
        //     from the loop, as those are the points where the control flow 
        //     needs to be properly merged back into the main program flow. 
        //     */
        //     if (loop->contains(succ)) continue;
        //     // find the corresponding cloned (perforated) version of the successor succ
        //     BasicBlock *perforatedSucc = cast<BasicBlock>(blockMap[succ]);
        //     term->setSuccessor(i, perforatedSucc);
        //   }
        // }
        // ##### Not sure if the cloned version also need modification #####

        // Problem faced:
        // 1. Branch condition - solved
        // 2. Perforation rate? currently set as must perforate (branch condition true) - constant power of 2
        // 2. Ensuring consistency after loop exits

        /** END OF TRANSFORMATION **/
      }

      return PreservedAnalyses::all();
    }
  };

  uint64_t CountFrequencyPass::total_freq = 0;
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "LoopPerforationPass", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
        ArrayRef<PassBuilder::PipelineElement>) {
          if(Name == "count-freq"){
            FPM.addPass(CountFrequencyPass());
            return true;
          }
          if(Name == "loop-perf"){
            FPM.addPass(LoopPerforationPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}
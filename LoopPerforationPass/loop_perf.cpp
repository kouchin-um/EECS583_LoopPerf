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

#include  <iostream>
#include  <unordered_set>
#include  <queue>

using namespace llvm;

#define THREASHOLD 0.2

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
        loop_info.emplace_back(LoopInfo{loop, instr_count});
      }

      std::vector<Loop*> loop_candidates;

      // The loops whose total frequency exceeds the threashold are selected as candidates
      for (auto &loop : loop_info) {
        if ((double)loop.instr_count / (double)CountFrequencyPass::total_freq > THREASHOLD){
          loop_candidates.emplace_back(loop.L);
          // errs() << format("%d / %d\n", loop.instr_count, CountFrequencyPass::total_freq);  
        }
      }


      /** Selective Perforation **/

      for (Loop* loop : loop_candidates) {
        std::unordered_set<Instruction*> all_instr; // all instructions in the loop

        /** SELECT BEGINS **/
        // select all loads
        std::unordered_set<Instruction*> target_instr;
        std::queue<Instruction*> instr_queue;
        for (BasicBlock *BB : loop->getBlocks()) {
          for (Instruction &I : *BB) {
            all_instr.emplace(&I);
            if (I.getOpcode() == Instruction::Load) {
              target_instr.emplace(&I);
              instr_queue.push(&I);
            }
          }
        }
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
              
              bool perforable = true;
              for (int i = 0; i < dest_instr->getNumOperands(); ++i) {
                Value* dest_source_value = dest_instr->getOperand(i);
                Instruction* dest_source_instr = dyn_cast<llvm::Instruction>(dest_source_value);
                if (target_instr.find(dest_source_instr) == target_instr.end()) {
                  // TODO: what about invariants?
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

        /** END OF EXPANSION **/

        // At this point, all the perforatable instructions are in
        // std::unordered_set<Instruction*> target_instr

        /** TRANFORMATION BEGINS **/



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

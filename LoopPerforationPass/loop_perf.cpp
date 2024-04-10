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

using namespace llvm;

namespace {

struct LoopPerforationPass : public PassInfoMixin<LoopPerforationPass> {

  typedef struct LoopInfoT {
        Loop* loop;
        uint64_t instr_count;
  } LoopInfo;

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
    llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);

    // Find the loops that have a lot of iterations and instructions
    std::vector<LoopInfo> loop_info;

    for (auto &loop : li.getTopLevelLoopsVector()) {
      uint64_t instr_count = 0;
      for (auto &bb : loop->getBlocksVector()) {
        uint64_t bb_freq = bfi.getBlockFreq(bb).getFrequency();
        uint64_t num_instr = std::distance(bb->begin(), bb->end());
        instr_count += bb_freq * num_instr;
      }
      loop_info.emplace_back(LoopInfo{loop, instr_count});
    }

    for (auto &loop : loop_info) {
      errs() << format("%d\n", loop.instr_count);  
    }
    return PreservedAnalyses::all();
  }
};
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "LoopPerforationPass", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
        ArrayRef<PassBuilder::PipelineElement>) {
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

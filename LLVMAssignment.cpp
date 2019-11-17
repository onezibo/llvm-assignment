//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//
#include <llvm/IR/Instructions.h> //注意有是Instructions.h不是Instruction.h
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Transforms/Scalar.h>
#include <map>
#include <string>
#include <vector>

#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#if LLVM_VERSION_MAJOR >= 4
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#else
#include <llvm/Bitcode/ReaderWriter.h>
#endif
using namespace llvm;
#if LLVM_VERSION_MAJOR >= 4
static ManagedStatic<LLVMContext> GlobalContext;
static LLVMContext &getGlobalContext() { return *GlobalContext; }
#endif
/* In LLVM 5.0, when  -O0 passed to clang , the functions generated with clang
 * will have optnone attribute which would lead to some transform passes
 * disabled, like mem2reg.
 */
#if LLVM_VERSION_MAJOR == 5
struct EnableFunctionOptPass : public FunctionPass {
  static char ID;
  EnableFunctionOptPass() : FunctionPass(ID) {}
  bool runOnFunction(Function &F) override {
    if (F.hasFnAttribute(Attribute::OptimizeNone)) {
      F.removeFnAttr(Attribute::OptimizeNone);
    }
    return true;
  }
};

char EnableFunctionOptPass::ID = 0;
#endif

///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
/// Updated 11/10/2017 by fargo: make all functions
/// processed by mem2reg before this pass.
struct FuncPtrPass : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  FuncPtrPass() : ModulePass(ID) {}

  std::vector<Value *> nodes;
  std::vector<Value *> n_set;
  std::map<Value *, Value *> edgs;
  std::vector<Value *> PVVs;
  std::map<Value *, Value *> PVCalls;
  std::map<Value *, std::vector<Value *>> PVBinds;
  std::map<Value *, std::vector<Value *>> PVVals;
  bool change;

  bool runOnModule(Module &M) override {
    errs() << "Hello: ";
    errs().write_escaped(M.getName()) << '\n';
    // M.dump();
    errs() << "------------------------------\n";
    //依次遍历到instruction
    for (Module::iterator it_mod = M.begin(), it_mod_e = M.end();
         it_mod != it_mod_e; it_mod++) {
      for (Function::iterator it_func = it_mod->begin(),
                              it_func_e = it_mod->end();
           it_func != it_func_e; it_func++) {
        // errs() << "Basic block name = " << it_func->getName().str() << "\n";
        for (BasicBlock::iterator it_bb = it_func->begin(),
                                  it_bb_e = it_func->end();
             it_bb != it_bb_e; it_bb++) {
          // errs() << *it_bb << "\n";
          Instruction *inst = &(*it_bb);
          // errs() << inst->getOpcode() << "  " <<  inst->getOpcodeName()
          // <<"\n";
          //判断是否为函数调用指令
          if (isa<CallInst>(inst) || isa<InvokeInst>(inst)) {
            CallInst *callinst = dyn_cast<CallInst>(inst);
            const Function *func = callinst->getCalledFunction();
            // 跳过 llvm.开头的函数
            if (func && func->isIntrinsic())
              continue;

            //直接调用
            if (func) {
              errs() << inst->getDebugLoc().getLine() << " : "
                     << func->getName() << '\n';
                     PVCalls
              PVCalls.insert(std::pair<Value*,Value*>(it_func->users().begin().getUse().get(),func->users().begin().getUse().get()));
              // PVCalls.insert(std::pair<Value*,Value*>(NULL,NULL));
            } else {
              //从间接调用获取类型，使用getCalledValue代替getCalledFunction
              // getFunction(callinst); //获取指令函数并打印
              Build_Call_Graph(callinst);
            }
          }
        }
      }
    }
    return false;
  }

  void getFunction(CallInst *callinst) {
    // errs() << " getFunction \n";
    Value *v = callinst->getCalledValue();
    Type *t = v->getType();
    FunctionType *ft =
        cast<FunctionType>(cast<PointerType>(t)->getElementType());
    if (ft == NULL)
      errs() << " hhhhhhhhhhhhhh \n";
    // ft->dump();
    Value *sv = v->stripPointerCasts();
    StringRef fname = sv->getName();
    errs() << "fname:" << fname << "\n";
    // auto val = callinst->ArgumentVal;
    // errs()<<"val = "<<val<<"\n";
    // test01.c
    // test05.c需对PHInode进行递归处理
    if (isa<PHINode>(v)) {
      errs() << "come to phinode\n";
      PHINode *phinode = dyn_cast<PHINode>(v);
      setPhinode(phinode, callinst);
    }
    //函数指针作为参数传入给函数
    else if (isa<Argument>(v)) {
      errs() << "come to argument\n";
      Argument *argument = dyn_cast<Argument>(v);
      // argument->dump();
      auto users = argument->getParent()->users();
      auto user_b = users.begin()->op_begin();
      auto user_e = users.begin()->op_end();
      // auto arguFunc_b = arguFunc->user_begin();
      // auto arguFunc_e = arguFunc->user_end();
      for (; user_b != user_e; user_b++) {
        // user_b->getUser()->dump();
        if (isa<PHINode>(user_b)) {
          errs() << "come to argument_PHINode\n";
          PHINode *func = dyn_cast<PHINode>(user_b);
          setPhinode(func, callinst);
        }
      }
    }
    // Value *callvalue = callinst->getCalledValue();
    // errs()<<"callinst ====== "<<*callinst<<"\n";
    // for(Use &U : callinst->operands()){
    //   Value *v = U.get();
    //   auto v_begin = v->user_begin();
    //   auto v_end = v->user_end();
    //   for(;v_begin!=v_end;v_begin++){
    //     //v_begin->dump();
    //     errs()<<"v->getName()"<<v_begin->getName()<<"\n";
    //     if(v_begin->getName().equals("call")){
    //       if(PHINode )
    //     }
    //   }
    // }
  }

  void setPhinode(PHINode *phinode, CallInst *callinst) {
    // errs()<<"!!!!!!!!!!!!!!!!!!!!!!!!!"<<"\n";
    // phinode->dump();
    // errs()<<"!!!!!!!!!!!!!!!!!!!!!!!!!"<<"\n";
    // StringRef phiname = phinode->getName();
    // StringRef opcodename = phinode->getOpcodeName();
    // phinode->getParent()->dump();
    llvm::User::op_range incomingval = phinode->incoming_values();
    auto *begin = incomingval.begin();
    auto *end = incomingval.end();
    for (; begin != end; begin++) {
      if (isa<PHINode>(begin)) {
        PHINode *temp = dyn_cast<PHINode>(begin);
        setPhinode(temp, callinst);
      } else if (isa<Function>(begin)) {
        Function *incomingfunc = dyn_cast<Function>(begin);
        errs() << callinst->getDebugLoc().getLine() << " : "
               << incomingfunc->getName() << "\n";
      }
    }
    // StringRef operandname =
    // phinode->getOperand(phinode->getNumOperands())->getName();
    // errs()<<"phiname = "<<phiname<<"\n";
    // errs()<<"opcodename = "<<opcodename<<"\n";
    // errs()<<"operandname = "<<operandname<<"\n";
  }

  void Build_Call_Graph(CallInst *root_procedure) {
    nodes.clear();
    n_set.clear();
    Value *v = root_procedure->getCalledValue();
    // v->dump();

    nodes.push_back(v);
    n_set.push_back(v);
    edgs.clear();
    change = true;
    for (int i = 0; i < n_set.size(); i++) {
      Value *v_temp = n_set[i];
      if (isa<PHINode>(v_temp)) {
        PHINode *phinode = dyn_cast<PHINode>(v_temp);
        std::vector<Value *> temp;
        temp.push_back(phinode);
        PVVals.insert(
            std::pair<Value *, std::vector<Value *>>(root_procedure, temp));
      } else if (isa<Argument>(v_temp)) {
        PHINode *phinode = dyn_cast<PHINode>(v_temp);
        std::vector<Value *> temp;
        temp.push_back(phinode);
        PVBinds.insert(
            std::pair<Value *, std::vector<Value *>>(root_procedure, temp));
      }
    }

    while (change) {
      change = false;
      bool more = true;
      while (more) {
        more = false;
        for (int i = 0; i < PVVs.size(); i++) {
          for (int j = i + 1; j < PVVs.size(); j++) {
            bool isinvoking;
            std::vector<Value *>::iterator it;
            it =
                find(PVBinds[PVVs[i]].begin(), PVBinds[PVVs[i]].end(), PVVs[i]);
            if (it != PVBinds[PVVs[i]].end()) {
              isinvoking = true;
            } else {
              isinvoking = false;
            }
            if (isinvoking && PVBinds[PVVs[i]] != PVBinds[PVVs[j]]) {
              std::vector<Value *> temp;
              sort(PVBinds[PVVs[i]].begin(), PVBinds[PVVs[i]].end());
              sort(PVBinds[PVVs[j]].begin(), PVBinds[PVVs[j]].end());
              temp.resize(PVBinds[PVVs[i]].size() + PVBinds[PVVs[j]].size());
              merge(PVBinds[PVVs[i]].begin(), PVBinds[PVVs[i]].end(),
                    PVBinds[PVVs[j]].begin(), PVBinds[PVVs[j]].end(),
                    temp.begin());
              PVBinds[PVVs[i]] = temp;
              more = true;
            }
          }
        }
      }
      std::map<Value *, Value *>::iterator iter;
      for (iter = PVCalls.begin(); iter != PVCalls.end(); iter++) {
        std::vector<Value *>::iterator it;
        for (it = PVVals[iter->second].begin();
             it != PVVals[iter->second].end(); it++) // iter->second为q
        {
          nodes.push_back(*it);
          edgs.insert(std::pair<Value *, Value *>(iter->first, *it));
          n_set.push_back(*it);
        }
        for (it = PVBinds[iter->second].begin();
             it != PVBinds[iter->second].end(); it++) {
          std::vector<Value *>::iterator it_v;
          for (it_v = PVVals[*it].begin(); it_v != PVVals[*it].end();
               it_v++) // iter->second为q
          {
            nodes.push_back(*it_v);
            edgs.insert(std::pair<Value *, Value *>(iter->first, *it_v));
            n_set.push_back(*it_v);
          }
        }
      }
    }
    if (edgs.empty())
      errs() << "edgs empty\n";
    std::map<Value *, Value *>::iterator iter;
    for (iter = edgs.begin(); iter != edgs.end(); iter++)

      errs() << iter->first->getName() << ' ' << iter->second->getName()
             << "\n";
  }
};

char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass",
                                   "Print function call instruction");

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<filename>.bc"), cl::init(""));

int main(int argc, char **argv) {
  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;
  // Parse the command line to read the Inputfilename
  cl::ParseCommandLineOptions(
      argc, argv, "FuncPtrPass \n My first LLVM too which does not do much.\n");

  // Load the input module
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  llvm::legacy::PassManager Passes;

/// Remove functions' optnone attribute in LLVM5.0
#if LLVM_VERSION_MAJOR == 5
  Passes.add(new EnableFunctionOptPass());
#endif
  /// Transform it to SSA
  Passes.add(llvm::createPromoteMemoryToRegisterPass());

  /// Your pass to print Function and Call Instructions
  Passes.add(new FuncPtrPass());
  Passes.run(*M.get());
}

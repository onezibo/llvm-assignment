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
#include <set>
#include <string>

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

  std::set<std::string> nodes;
  std::set<std::string> n_set;
  std::map<std::string, std::string> edgs;
  std::set<std::string> PVVs;
  std::map<std::string, std::string> PVCalls;
  std::map<std::string, std::string> PVBinds;
  std::map<std::string, std::set<std::string>> PVVals;
  std::map<std::string, CallInst *> stoc;
  std::map<std::string, Function *> stof;
  std::map<unsigned int, std::set<std::string>> line;
  std::map<std::string, std::string> functoret;
  bool change;

  bool runOnModule(Module &M) override {
    // errs() << "Hello: ";
    // errs().write_escaped(M.getName()) << '\n';
    // M.dump();
    // errs() << "------------------------------\n";
    //依次遍历到instruction
    for (Module::iterator it_mod = M.begin(), it_mod_e = M.end();
         it_mod != it_mod_e; it_mod++) {
      stof.insert(
          std::pair<std::string, Function *>(it_mod->getName(), &*it_mod));
      Function *f = &(*it_mod);
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
          if (isa<ReturnInst>(inst)) {
            ReturnInst *ri = dyn_cast<ReturnInst>(inst);
            functoret.insert(std::pair<std::string, std::string>(
                f->getName(), ri->getReturnValue()->getName()));
            errs() << "fuc " << f->getName() << "   return "
                   << ri->getReturnValue()->getName() << "\n";
          }
          if (isa<PHINode>(inst)) {
            PHINode *phi_t = dyn_cast<PHINode>(inst);
            errs() << "hhhhhhhhhhhh     " << inst->getName() << "\n";
            // PVBinds.insert();
          }
          //判断是否为函数调用指令
          if (isa<CallInst>(inst) || isa<InvokeInst>(inst)) {
            CallInst *callinst = dyn_cast<CallInst>(inst);
            int i = 0;
            for (User::op_iterator arg = callinst->arg_begin();
                 arg != callinst->arg_end(); arg++, i++) {
              if (arg->get()->getType()->isPointerTy()) {
                errs() << "-----------------------------\n";
                errs() << "call arg name " << arg->get()->getName() << "\n";
                // errs()<<"call user name "<<arg->getUser()->getName()<<"\n";
                // errs() << "call getcallee name "
                //        << callinst->getCalledValue()->getName() << "\n";
                Function *f = stof[callinst->getCalledValue()->getName()];
                Argument *arg_initial = f->arg_begin();
                int j = 0;
                while (arg_initial != f->arg_end() && i != j) {
                  arg_initial++;
                  j++;
                }

                if (arg_initial->getType()->isPointerTy()) {
                  // errs() << "ffffffffffffffffffffffffff\n";
                  // for (Value::user_iterator u = arg->user_begin();
                  //      u != arg->user_end(); u++)
                  //   errs() << "arg user name " << u->getName() << "\n";
                  for (Value::use_iterator us = arg_initial->use_begin();
                       us != arg_initial->use_end(); us++) {
                    errs() << "arg user use name " << us->get()->getName()
                           << "\n";
                    if (PVBinds.count(us->get()->getName())) {
                      PVBinds[us->get()->getName()] = arg->get()->getName();
                    } else
                      PVBinds.insert(std::pair<std::string, std::string>(
                          us->get()->getName(), arg->get()->getName()));
                  }
                }

                // for(Value::user_iterator u =
                // arg->user_begin();u!=arg->user_end(); u++ )
                //   errs()<<"arg user name "<<u->getName()<<"\n";
                // for(Value::use_iterator us = arg->use_begin();
                // us!=arg->use_end();us++ )
                //   errs()<<"arg user use name "<<us->get()->getName()<<"\n";
              }
            }

            Function *func = callinst->getCalledFunction();
            // 跳过 llvm.开头的函数
            if (func && func->isIntrinsic())
              continue;

            //直接调用
            if (func) {
              // errs() << inst->getDebugLoc().getLine() << " : "
              //        << func->getName() << '\n';
              PVCalls.insert(std::pair<std::string, std::string>(
                  it_func->getName(), func->getName()));
              std::set<std::string> func_temp;
              func_temp.insert(func->getName());
              line.insert(std::pair<unsigned int, std::set<std::string>>(
                  inst->getDebugLoc().getLine(), func_temp));

              stoc.insert(std::pair<std::string, CallInst *>(func->getName(),
                                                             callinst));
              // PVCalls.insert(std::pair<Value*,Value*>(NULL,NULL));
            } else {
              //从间接调用获取类型，使用getCalledValue代替getCalledFunction
              Value *v = callinst->getCalledValue();
              stoc.insert(
                  std::pair<std::string, CallInst *>(v->getName(), callinst));
              getFunction(v, callinst); //获取指令函数并打印
              // errs() << "it_mod->getName()==" << it_mod->getName() << " "
              //        << "callinst->getCalledValue()->getName()===="
              //        << callinst->getCalledValue()->getName() << "\n";
              PVVs.insert(callinst->getCalledValue()->getName());
              PVCalls.insert(std::pair<std::string, std::string>(
                  it_mod->getName(), callinst->getCalledValue()->getName()));
              // std::set<std::string> func_temp;
              // if (!line.empty())
              //   func_temp = line[callinst->getDebugLoc().getLine()];
              // func_temp.insert(callinst->getCalledValue()->getName());
              // line.insert(std::pair<unsigned int, std::set<std::string>>(
              //     callinst->getDebugLoc().getLine(), func_temp));
              Build_Call_Graph(callinst);
            }
          }
        }
      }
    }

    std::map<unsigned int, std::set<std::string>>::iterator it_line;
    for (it_line = line.begin(); it_line != line.end(); it_line++) {
      std::set<std::string>::iterator it_fl = it_line->second.begin();

      errs() << it_line->first << " :";
      for (; it_fl != it_line->second.end();) {
        if (PVBinds.count(*it_fl)) {
          errs() << " " << PVBinds[*it_fl];
        } else
          errs() << " " << *it_fl;
        if ((++it_fl) != it_line->second.end()) {
          errs() << ",";
        }
      }
      errs() << "\n";
    }
    return false;
  }

  // void process_pvbind() {
  //   PVBinds.insert(std::pair<std::string, std::string>());
  // }

  void getFunction(Value *v, CallInst *callinst) {
    // // errs() << " getFunction \n";
    // Value *v = callinst->getCalledValue();
    Type *t = v->getType();
    FunctionType *ft =
        cast<FunctionType>(cast<PointerType>(t)->getElementType());
    // if (ft == NULL)
    //   errs() << " hhhhhhhhhhhhhh \n";
    // ft->dump();
    Value *sv = v->stripPointerCasts();
    StringRef fname = sv->getName();
    // errs() << "fname:" << fname << "\n";
    // auto val = callinst->ArgumentVal;
    // errs()<<"val = "<<val<<"\n";
    // test01.c
    // test05.c需对PHInode进行递归处理
    if (isa<PHINode>(v)) {
      // errs() << "come to phinode\n";
      PHINode *phinode = dyn_cast<PHINode>(v);
      setPhinode(phinode, callinst);
    } else if (isa<CallInst>(v)) {
      // errs() << "cccccccccccccccccccccccc\n";
      auto ci = dyn_cast<CallInst>(v);
      // errs() << "callinst name    " << v->getName() << "\n";
      errs() << "call name    " << ci->getCalledValue()->getName() << "\n";
      std::string ret = functoret[ci->getCalledValue()->getName()];
      std::string s = PVBinds[ret];
      std::map<std::string, std::string>::iterator it_p;
      for (it_p = PVBinds.begin(); it_p != PVBinds.end(); it_p++) {
        errs() << "!!!!!!!!!         " << it_p->first << "     " << it_p->second
               << "          "
               << "\n";
      }
      setLine(callinst, s);

      // CallInst *inst = dyn_cast<CallInst>(v);
      // Value *return_v = callinst->getCalledValue();
      // getFunction(return_v, callinst);
      // process(inst);
    }
    //函数指针作为参数传入给函数
    else if (isa<Argument>(v)) {
      // errs() << "come to argument\n";
      Argument *argument = dyn_cast<Argument>(v);
      // argument->dump();
      auto users = argument->getParent()->users();
      for (auto user = users.begin(); user != users.end(); user++) {
        // errs() << "user name : " << user->getName() << "\n";
        auto user_b = user->op_begin();
        auto user_e = user->op_end();
        // auto arguFunc_b = arguFunc->user_begin();
        // auto arguFunc_e = arguFunc->user_end();
        for (; user_b != user_e; user_b++) {
          // user_b->getUser()->dump();
          if (isa<PHINode>(user_b)) {
            // errs() << "come to argument_PHINode\n";
            PHINode *func = dyn_cast<PHINode>(user_b);
            setPhinode(func, callinst);
          } else if (isa<Argument>(user_b) &&
                     (user_b->get()->getType()->isPointerTy())) {
            Value *v = user_b->get();
            getFunction(v, callinst);
          }
        }
      }
    }
  }

  void setLine(CallInst *callinst, std::string s) {
    std::set<std::string> func_temp;
    if (line.find(callinst->getDebugLoc().getLine()) != line.end()) {
      func_temp = line[callinst->getDebugLoc().getLine()];
      func_temp.insert(s);
      line[callinst->getDebugLoc().getLine()] = func_temp;
    } else {
      func_temp.insert(s);
      line.insert(std::pair<unsigned int, std::set<std::string>>(
          callinst->getDebugLoc().getLine(), func_temp));
    }
  }

  std::set<std::string> setPhinode(PHINode *phinode, CallInst *callinst) {
    llvm::User::op_range incomingval = phinode->incoming_values();
    auto *begin = incomingval.begin();
    auto *end = incomingval.end();
    std::set<std::string> result;
    for (; begin != end; begin++) {
      if (isa<PHINode>(begin)) {
        PHINode *temp = dyn_cast<PHINode>(begin);
        std::set<std::string> x;
        std::set_union(
            result.begin(), result.end(), setPhinode(temp, callinst).begin(),
            setPhinode(temp, callinst).end(), std::inserter(x, x.begin()));
        result = x;
        // setPhinode(temp, callinst);
      } else if (isa<Function>(begin)) {
        Function *incomingfunc = dyn_cast<Function>(begin);
        result.insert(callinst->getCalledValue()->getName());
        return result;
        // if (PVVals.count(callinst->getCalledValue()->getName())) {
          // PVVals[callinst->getCalledValue()->getName()].insert(
          //     incomingfunc->getName());
          // PVVals.insert(std::pair<std::string, std::set<std::string>>(
          //     callinst->getCalledValue()->getName(),
          //     PVVals[callinst->getCalledValue()->getName()]));
          // setLine(callinst, incomingfunc->getName());
          // std::set<std::string> func_temp;
          // if (line.find(callinst->getDebugLoc().getLine()) !=
          //     line.end()) { // if (!line.empty())
          //   func_temp = line[callinst->getDebugLoc().getLine()];
          //   func_temp.insert(incomingfunc->getName());
          //   line[callinst->getDebugLoc().getLine()] = func_temp;
          // } else {
          //   func_temp.insert(incomingfunc->getName());
          //   line.insert(std::pair<unsigned int, std::set<std::string>>(
          //       callinst->getDebugLoc().getLine(), func_temp));
          // }
          // errs() << "-----------incomingfunc->getName()---------"
          //        << incomingfunc->getName() << "\n";
        // } else {
        //   std::set<std::string> temp;
        //   temp.insert(incomingfunc->getName());
        //   PVVals.insert(std::pair<std::string, std::set<std::string>>(
        //       callinst->getCalledValue()->getName(), temp));
          // setLine(callinst, incomingfunc->getName());
          // std::set<std::string> func_temp;
          // if (line.find(callinst->getDebugLoc().getLine()) !=
          //     line.end()) { // if (!line.empty())
          //   func_temp = line[callinst->getDebugLoc().getLine()];
          //   func_temp.insert(incomingfunc->getName());
          //   line[callinst->getDebugLoc().getLine()] = func_temp;
          // } else {
          //   func_temp.insert(incomingfunc->getName());
          //   line.insert(std::pair<unsigned int, std::set<std::string>>(
          //       callinst->getDebugLoc().getLine(), func_temp));
          // }
          // errs() << "-----------incomingfunc->getName()---------"
          //        << incomingfunc->getName() << "\n";
        // }
      } else if (isa<Argument>(begin)) {
        // getFunction(begin->get(), callinst);
        result.insert(begin->get()->getName());
        return result;
      }

      // errs() << callinst->getDebugLoc().getLine() << " : "
      //        << incomingfunc->getName() << "\n";
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

    nodes.insert(v->getName());
    n_set.insert(v->getName());
    edgs.clear();
    change = true;
    std::set<std::string>::iterator it_ns;
    for (it_ns = n_set.begin(); it_ns != n_set.end(); it_ns++) {
      std::string v_temp = *it_ns;
      // errs() << "v_temp" << v_temp << "\n";
      Value *v_temp_func = stoc[v_temp];
      // if (v_temp_func == NULL)
      //   errs() << "hhhhhhhhhhh\n";
      // errs() << "v_temp_func->getName()" << v_temp_func->getName() << "\n";
      if (isa<PHINode>(v_temp_func)) {
        PHINode *phinode = dyn_cast<PHINode>(v_temp_func);
        std::set<std::string> temp;
        temp.insert(phinode->getName());
        PVVals.insert(std::pair<std::string, std::set<std::string>>(
            root_procedure->getName(), temp));
      } else if (isa<Argument>(v_temp_func)) {
        PHINode *phinode = dyn_cast<PHINode>(v_temp_func);
        std::set<std::string> temp;
        temp.insert(phinode->getName());
        PVBinds.insert(std::pair<std::string, std::string>(
            root_procedure->getName(), phinode->getName()));
      }
    }

    while (change) {
      change = false;
      bool more = true;

      // while (more) {
      //   more = false;
      //   for (int i = 0; i < PVVs.size(); i++) {
      //     for (int j = i + 1; j < PVVs.size(); j++) {
      //       bool isinvoking;
      //       std::set<std::string>::iterator it;
      //       it =
      //           find(PVBinds[PVVs[i]].begin(), PVBinds[PVVs[i]].end(),
      //           PVVs[i]);
      //       if (it != PVBinds[PVVs[i]].end()) {
      //         isinvoking = true;
      //       } else {
      //         isinvoking = false;
      //       }
      //       if (isinvoking && PVBinds[PVVs[i]] != PVBinds[PVVs[j]]) {
      //         std::set<std::string> temp;
      //         sort(PVBinds[PVVs[i]].begin(), PVBinds[PVVs[i]].end());
      //         sort(PVBinds[PVVs[j]].begin(), PVBinds[PVVs[j]].end());
      //         temp.resize(PVBinds[PVVs[i]].size() + PVBinds[PVVs[j]].size());
      //         merge(PVBinds[PVVs[i]].begin(), PVBinds[PVVs[i]].end(),
      //               PVBinds[PVVs[j]].begin(), PVBinds[PVVs[j]].end(),
      //               temp.begin());
      //         PVBinds[PVVs[i]] = temp;
      //         more = true;
      //       }
      //     }
      //   }
      // }

      std::map<std::string, std::string>::iterator iter;
      for (iter = PVCalls.begin(); iter != PVCalls.end(); iter++) {
        std::set<std::string>::iterator it;
        for (it = PVVals[iter->second].begin();
             it != PVVals[iter->second].end(); it++) // iter->second为q
        {
          nodes.insert(*it);
          edgs.insert(std::pair<std::string, std::string>(iter->first, *it));
          n_set.insert(*it);
        }
        // for (it = PVBinds[iter->second].begin();
        //      it != PVBinds[iter->second].end(); it++) {
        //   std::set<std::string>::iterator it_v;
        //   for (it_v = PVVals[*it].begin(); it_v != PVVals[*it].end();
        //        it_v++) // iter->second为q
        //   {
        //     nodes.insert(*it_v);
        //     edgs.insert(
        //         std::pair<std::string, std::string>(iter->first, *it_v));
        //     n_set.insert(*it_v);
        //   }
        // }
      }
    }
    // if (edgs.empty())
    //   errs() << "edgs empty\n";
    // std::map<std::string, std::string>::iterator iter;
    // for (iter = edgs.begin(); iter != edgs.end(); iter++)
    //   errs() << iter->first << ' ' << iter->second.c_str() << "\n";
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

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

  std::map<std::string, std::set<std::string>> PVVals;
  std::map<std::string, CallInst *> stoc;
  std::map<std::string, Function *> stof;
  std::map<unsigned int, std::set<std::string>> line;
  std::map<std::string, std::string> functoret;
  std::map<std::string, std::string> valtoaddr;
  bool change;

  std::set<Function *> funsfromname(std::string s) {
    std::set<Function *> res;
    if (stof.find(s) == stof.end()) {
      assert(PVVals.find(s) != PVVals.end());
      std::set<std::string>::iterator pvit = PVVals[s].begin();
      for (; pvit != PVVals[s].end(); pvit++) {
        std::set<Function *> tmp;
        std::set<Function *> phiset = funsfromname(*pvit);
        set_union(phiset.begin(), phiset.end(), res.begin(), res.end(),
                  inserter(tmp, tmp.begin()));
        res = tmp;
      }
    } else {
      res.insert(stof[s]);
    }
    return res;
  }
  std::set<std::string> getvvals(std::set<std::string> oldset) {
    bool change = true;
    std::set<std::string> newset = oldset;
    std::set<std::string> tmp = newset;
    std::set<std::string> vset;
    while (change) {
      change = false;
      std::set<std::string>::iterator it_cs;
      for (it_cs = newset.begin(); it_cs != newset.end(); it_cs++) {
        if (PVVals.find(*it_cs) != PVVals.end()) {
          vset = PVVals[*it_cs];
          tmp = set_union_str(tmp, vset);
          tmp.erase(*it_cs);
        }
      }
      newset = tmp;
      if (newset != oldset) {
        oldset = newset;
        change = true;
      }
    }

    return newset;
  }
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
        for (BasicBlock::iterator it_bb = it_func->begin(),
                                  it_bb_e = it_func->end();
             it_bb != it_bb_e; it_bb++) {
          Instruction *inst = &(*it_bb);
          if (isa<StoreInst>(inst)) {
            StoreInst *storei = dyn_cast<StoreInst>(inst);
            std::set<std::string> val;
            std::set<std::string> pvals;

            if (valtoaddr.find(storei->getPointerOperand()->getName()) !=
                valtoaddr.end())
              val.insert(valtoaddr[storei->getPointerOperand()->getName()]);
            else
              val.insert(storei->getPointerOperand()->getName());
            pvals.insert(storei->getValueOperand()->getName());
            std::set<std::string> addr = getvvals(val);

            for (std::set<std::string>::iterator addrit = addr.begin();
                 addrit != addr.end(); addrit++)
              pvvals(*addrit, pvals);
          }
          if (isa<LoadInst>(inst)) {
            LoadInst *ldi = dyn_cast<LoadInst>(inst);
            std::string addr;
            if (valtoaddr.find(ldi->getPointerOperand()->getName()) !=
                valtoaddr.end())
              addr = valtoaddr[ldi->getPointerOperand()->getName()];
            else
              addr = ldi->getPointerOperand()->getName();
            std::set<std::string> pvals;
            std::set<std::string> addrset;
            addrset.insert(addr);
            pvals = getvvals(addrset);
            pvvals(ldi->getName(), pvals);
          }
          if (isa<GetElementPtrInst>(inst)) {
            GetElementPtrInst *getptr = dyn_cast<GetElementPtrInst>(inst);
            valtoaddr.insert(std::pair<std::string, std::string>(
                getptr->getName(), getptr->getPointerOperand()->getName()));
          }
          if (isa<ReturnInst>(inst)) {
            ReturnInst *ri = dyn_cast<ReturnInst>(inst);
            functoret.insert(std::pair<std::string, std::string>(
                f->getName(), ri->getReturnValue()->getName()));
          }
          if (isa<PHINode>(inst)) {
            PHINode *phi_t = dyn_cast<PHINode>(inst);
            std::set<std::string> vals = setPhinode(phi_t);
            pvvals(inst->getName(), vals);
          }
          //判断是否为函数调用指令
          if (isa<CallInst>(inst) || isa<InvokeInst>(inst)) {
            CallInst *callinst = dyn_cast<CallInst>(inst);
            int i = 0;
            for (User::op_iterator arg = callinst->arg_begin();
                 arg != callinst->arg_end(); arg++, i++) {
              if (arg->get()->getType()->isPointerTy()) {
                std::set<Function *> fset =
                    funsfromname(callinst->getCalledValue()->getName());
                std::set<Function *>::iterator fit;
                for (fit = fset.begin(); fit != fset.end(); fit++) {
                  Function *f = *fit;
                  Argument *arg_initial = f->arg_begin();
                  int j = 0;
                  while (arg_initial != f->arg_end() && i != j) {
                    arg_initial++;
                    j++;
                  }
                  if (arg_initial == f->arg_end())
                    continue;
                  if (arg_initial->getType()->isPointerTy()) {
                    for (Value::use_iterator us = arg_initial->use_begin();
                         us != arg_initial->use_end(); us++) {
                      std::set<std::string> vals;
                      vals.insert(arg->get()->getName());
                      pvvals(us->get()->getName(), vals);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    processmod(M);
  }
  void processmod(Module &M) {
    for (Module::iterator it_mod = M.begin(), it_mod_e = M.end();
         it_mod != it_mod_e; it_mod++) {
      for (Function::iterator it_func = it_mod->begin(),
                              it_func_e = it_mod->end();
           it_func != it_func_e; it_func++) {
        for (BasicBlock::iterator it_bb = it_func->begin(),
                                  it_bb_e = it_func->end();
             it_bb != it_bb_e; it_bb++) {
          // errs() << *it_bb << "\n";
          Instruction *inst = &(*it_bb);
          CallInst *callinst = dyn_cast<CallInst>(inst);
          int i = 0;
          if (isa<CallInst>(inst) || isa<InvokeInst>(inst)) {
            Function *func = callinst->getCalledFunction();
            // 跳过 llvm.开头的函数
            if (func && func->isIntrinsic())
              continue;

            //直接调用
            if (func) {
              setLine(callinst, func->getName());
              stoc.insert(std::pair<std::string, CallInst *>(func->getName(),
                                                             callinst));
            } else {
              //从间接调用获取类型，使用getCalledValue代替getCalledFunction
              Value *v = callinst->getCalledValue();

              stoc.insert(
                  std::pair<std::string, CallInst *>(v->getName(), callinst));
              std::set<std::string> callset = getFunction(v);
              std::set<std::string>::iterator it_cs;

              //获取指令函数并打印
              callset = getvvals(callset);
              setLine(callinst, callset);
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
        errs() << " " << *it_fl;
        if ((++it_fl) != it_line->second.end()) {
          errs() << ",";
        }
      }
      errs() << "\n";
    }
  }

  std::set<std::string> getFunction(Value *v) {
    std::set<std::string> res;
    // test01.c
    // test05.c需对PHInode进行递归处理
    if (isa<LoadInst>(v)) {
      LoadInst *ldi = dyn_cast<LoadInst>(v);
      res.insert(ldi->getName());
    }
    if (isa<PHINode>(v)) {
      PHINode *phinode = dyn_cast<PHINode>(v);
      res = setPhinode(phinode);
    } else if (isa<CallInst>(v)) {
      auto ci = dyn_cast<CallInst>(v);
      std::set<std::string> callset;
      callset.insert(ci->getCalledValue()->getName());
      callset = getvvals(callset);
      std::set<std::string> retset;
      std::set<std::string>::iterator ics;
      for (ics = callset.begin(); ics != callset.end(); ics++) {
        std::string ret = functoret[*ics];
        retset.insert(ret);
      }
      res = retset;
    }
    //函数指针作为参数传入给函数
    else if (isa<Argument>(v)) {
      Argument *argument = dyn_cast<Argument>(v);
      res.insert(argument->getName());
    }
    return res;
  }

  std::set<std::string> set_union_str(std::set<std::string> a,
                                      std::set<std::string> b) {
    std::set<std::string> uniset;
    std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                   std::inserter(uniset, uniset.begin()));
    return uniset;
  }

  void pvvals(std::string s, std::set<std::string> sset) {
    // errs() << "insert pv   " << s << "\n";
    std::set<std::string> uniset;
    std::set<std::string> func_temp;
    if (PVVals.find(s) != PVVals.end()) {
      func_temp = PVVals[s];
      uniset = set_union_str(func_temp, sset);
      PVVals[s] = uniset;
    } else {
      uniset = sset;
      PVVals.insert(std::pair<std::string, std::set<std::string>>(s, uniset));
    }
  }

  void setLine(CallInst *callinst, std::set<std::string> sset) {
    std::set<std::string> uniset;
    std::set<std::string> func_temp;
    if (line.find(callinst->getDebugLoc().getLine()) != line.end()) {
      func_temp = line[callinst->getDebugLoc().getLine()];
      uniset = set_union_str(func_temp, sset);
      line[callinst->getDebugLoc().getLine()] = uniset;
    } else {
      uniset = sset;
      line.insert(std::pair<unsigned int, std::set<std::string>>(
          callinst->getDebugLoc().getLine(), uniset));
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

  std::set<std::string> setPhinode(PHINode *phinode) {
    llvm::User::op_range incomingval = phinode->incoming_values();
    auto *begin = incomingval.begin();
    auto *end = incomingval.end();
    std::set<std::string> result;
    for (; begin != end; begin++) {
      if (isa<PHINode>(begin)) {
        PHINode *temp = dyn_cast<PHINode>(begin);
        std::set<std::string> nextphi = setPhinode(temp);
        result = set_union_str(result, nextphi);
      } else if (isa<Function>(begin)) {
        Function *incomingfunc = dyn_cast<Function>(begin);
        result.insert(incomingfunc->getName());

      } else if (isa<Argument>(begin)) {
        // getFunction(begin->get());
        result.insert(begin->get()->getName());
      }
    }
    return result;
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

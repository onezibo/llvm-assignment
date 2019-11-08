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

#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/IR/Instructions.h>//注意有是Instructions.h不是Instruction.h
#include <llvm/Transforms/Scalar.h>

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
/* In LLVM 5.0, when  -O0 passed to clang , the functions generated with clang will
 * have optnone attribute which would lead to some transform passes disabled, like mem2reg.
 */
#if LLVM_VERSION_MAJOR == 5
struct EnableFunctionOptPass: public FunctionPass {
    static char ID;
    EnableFunctionOptPass():FunctionPass(ID){}
    bool runOnFunction(Function & F) override{
        if(F.hasFnAttribute(Attribute::OptimizeNone))
        {
            F.removeFnAttr(Attribute::OptimizeNone);
        }
        return true;
    }
};

char EnableFunctionOptPass::ID=0;
#endif

	
///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
///Updated 11/10/2017 by fargo: make all functions
///processed by mem2reg before this pass.
struct FuncPtrPass : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  FuncPtrPass() : ModulePass(ID) {}

  
  bool runOnModule(Module &M) override {
    errs() << "Hello: ";
    errs().write_escaped(M.getName()) << '\n';
    M.dump();
    errs()<<"------------------------------\n";
    //依次遍历到instruction
    for(Module::iterator it_mod = M.begin(),it_mod_e = M.end();it_mod!=it_mod_e;it_mod++){
      for(Function::iterator it_func = it_mod->begin(),it_func_e = it_mod->end();it_func!=it_func_e;it_func++){
        //errs() << "Basic block name=" << it_func->getName().str() << "\n";
        for(BasicBlock::iterator it_bb=it_func->begin(),it_bb_e=it_func->end();it_bb!=it_bb_e;it_bb++){
          //outs() << *it_bb << "\n";
          Instruction *inst = &(*it_bb);
          //errs() << inst->getName() << "    " << inst->getOpcode() << "\n";
          //判断是否为函数调用指令
          if(isa<CallInst>(inst) || isa<InvokeInst>(inst)){
            CallInst *callinst = dyn_cast<CallInst>(inst);
            const Function *func = callinst->getCalledFunction();
            //直接调用
            if(func){
              errs() << inst->getDebugLoc().getLine() << " : "<< func->getName() << '\n';
            }
            else{
              //从间接调用获取类型，使用getCalledValue代替getCalledFunction
              getFunction(callinst);//获取指令函数并打印
            }
          }
        }
      }
    }
    return false;
  }

  void getFunction(CallInst *callinst){
    Type* t = callinst->getCalledValue()->getType(); 
    FunctionType* ft = cast<FunctionType>(cast<PointerType>(t)->getElementType()); 
    ft->dump();
    Value *v = callinst->getCalledValue();
    Value* sv = v->stripPointerCasts();
    StringRef fname = sv->getName();
    errs()<<"fname:"<<fname<<"\n";
    // auto val = callinst->ArgumentVal;
    // errs()<<"val = "<<val<<"\n";
    //test01.c
    if(isa<PHINode>(v)){
      PHINode *phinode = dyn_cast<PHINode>(v);
      // errs()<<"!!!!!!!!!!!!!!!!!!!!!!!!!"<<"\n";
      // phinode->dump();
      // errs()<<"!!!!!!!!!!!!!!!!!!!!!!!!!"<<"\n";
      // StringRef phiname = phinode->getName();
      // StringRef opcodename = phinode->getOpcodeName();
      //phinode->getParent()->dump();
      auto incomingval = phinode->incoming_values();
      auto *begin = incomingval.begin();
      auto *end = incomingval.end();
      for(;begin!=end;begin++){
        if(isa<Function>(begin)){
          Function *incomingfunc = dyn_cast<Function>(begin);
          errs()<< callinst->getDebugLoc().getLine() << " : "<< incomingfunc->getName()<<"\n";
        }
      }
      //StringRef operandname = phinode->getOperand(phinode->getNumOperands())->getName();
      // errs()<<"phiname = "<<phiname<<"\n";
      // errs()<<"opcodename = "<<opcodename<<"\n";
      //errs()<<"operandname = "<<operandname<<"\n";
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
};


char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass", "Print function call instruction");

static cl::opt<std::string>
InputFilename(cl::Positional,
              cl::desc("<filename>.bc"),
              cl::init(""));


int main(int argc, char **argv) {
   LLVMContext &Context = getGlobalContext();
   SMDiagnostic Err;
   // Parse the command line to read the Inputfilename
   cl::ParseCommandLineOptions(argc, argv,
                              "FuncPtrPass \n My first LLVM too which does not do much.\n");


   // Load the input module
   std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
   if (!M) {
      Err.print(argv[0], errs());
      return 1;
   }

   llvm::legacy::PassManager Passes;
   	
   ///Remove functions' optnone attribute in LLVM5.0
   #if LLVM_VERSION_MAJOR == 5
   Passes.add(new EnableFunctionOptPass());
   #endif
   ///Transform it to SSA
   Passes.add(llvm::createPromoteMemoryToRegisterPass());

   /// Your pass to print Function and Call Instructions
   Passes.add(new FuncPtrPass());
   Passes.run(*M.get());
}


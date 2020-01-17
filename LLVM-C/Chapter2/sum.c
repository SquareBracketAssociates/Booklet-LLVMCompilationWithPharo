/**
 * LLVM equivalent of:
 *
 * int sum(int a, int b) {
 *     return a + b;
 * }
 */

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    // Module creation
    LLVMModuleRef mod = LLVMModuleCreateWithName("my_module");

    // Function prototype creation
    LLVMTypeRef param_types[] = { LLVMInt32Type(), LLVMInt32Type() };
    LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types, 2, 0);
    LLVMValueRef sum = LLVMAddFunction(mod, "sum", ret_type);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(sum, "entry");

    // Builder creation
    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, entry);

    // Instruction added to the builder
    LLVMValueRef tmp = LLVMBuildAdd(builder, LLVMGetParam(sum, 0), LLVMGetParam(sum, 1), "tmp");
    LLVMBuildRet(builder, tmp);

    //Analysis
    char *error = NULL;
    LLVMVerifyModule(mod, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);

    // Bitcode writing to file
    if (LLVMWriteBitcodeToFile(mod, "sum.bc") != 0) {
        fprintf(stderr, "error writing bitcode to file, skipping\n");
    }
    // Bitcode writing to memory buffer
    LLVMMemoryBufferRef mem = LLVMWriteBitcodeToMemoryBuffer(mod);
    //
    // // Choosing the triple
    char triple[] = "x86_64";
    char cpu[] = "";
    printf("%s\n",triple);

    // Initialization of the targets
    LLVMTargetRef* targetRef;

    // Generating the target machine
    char** errPtrTriple;
    LLVMBool res = LLVMGetTargetFromTriple(triple, targetRef, errPtrTriple);
    if (res == 1)
    {
        printf("%s\n",*errPtrTriple);
    }
    printf("%d\n",res);

    /*LLVMTargetMachineRef T = LLVMCreateTargetMachine(LLVMTargetRef T,
    *                                                 const char* Triple,
    *                                                 const char* CPU,
    *                                                 const char* features,
    *                                                 LLVMCodeGenOptLevel Level,
    *                                                 LLVMRelocMode Reloc,
    *                                                LLVMCodeModel CodeModel);
    */

    LLVMTargetMachineRef targetMachineRef = LLVMCreateTargetMachine(*targetRef, triple, cpu, "", LLVMCodeGenLevelNone, LLVMRelocDefault, LLVMCodeModelDefault);

    // Bitcode writing to file
    // LLVMTargetMachineEmitToFile(LLVMTargetMachineRef T, LLVMModuleRef M, char* filename, LLVMCodeGenFileType codegen, char** ErrorMessage)
    char** errPtrFile;
    LLVMTargetMachineEmitToFile(targetMachineRef, mod, "spec_sum.bc", LLVMObjectFile, errPtrFile);

    // Bitcode writing to memory buffer
    // LLVMTargetMachineEmitToMemoryBuffer(LLVMTargetMachineRef T, LLVMModuleRef M, LLVMCodeGenFileType codegen, char** ErrorMessage, LLVMMemoryBufferRef OutMemBuf)
    char** errPtrMem;
    LLVMTargetMachineEmitToMemoryBuffer(targetMachineRef, mod, LLVMObjectFile, errPtrMem, &mem);

    LLVMDisposeBuilder(builder);
}

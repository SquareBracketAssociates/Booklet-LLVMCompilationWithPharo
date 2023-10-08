## First function in LLVM IR


### Presentation

In this chapter, we will be using the LLVM C bindings to build an in-memory
representation of an extremely simple function. In order to do so, we will work
on a `sum` function whose C equivalent would be:

```language=c
int sum(int a, int b) {
    return a + b;
}
```


To use the LLVM C bindings, we will define the succession of functions
to be used in a C program. We will perform in a manual way what an actual compiler
could do to an Abstract Syntax Tree result of the function.

### Base LLVM components


#### Module

The top-level structure in an LLVM program is a module. The official documentation
describes this element as a translation unit or a collection of translation units
merged together which basically means the module will keep track of functions, global
variables, external references and symbols. They are the top-level container for
all things defined in LLVM. We can create one by using the `LLVMModuleCreateWithName()` function as follows:

```language=c
LLVMModuleRef mod = LLVMModuleCreateWithName("llvm_c_tutorial");
```


#### Types

In order to create the `sum` function and add it to the module, we need the
following components:

- its return type
- its parameters type \(vector\)
- a set of basic blocks


Let's first look at the prototype of the function \(return and parameters type\).
Those can be defined in LLVM using the `LLVMTypeRef` holding all the types
of the LLVM IR.

![LLVM Type Hierarchy](figures/llvm_types.png)

Using those types, we can now build our function prototype. Starting with the
parameters type, we can define an `LLVMTypeRef` vector with two `int32` instances created using `LLVMInt32Type()`:

```language=c
LLVMTypeRef param_types[] = { LLVMInt32Type(), LLVMInt32Type() };
```


Note that all the types in LLVM can be used with the LLVM<type>Type\(\) constructor.
This is the case with our two `LLVMInt32Type()`. Next comes the function type
that is defined as follows:

```language=c
LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types, 2, 0);
```


The `LLVMFunctionType()` constructor takes for arguments:
1. the function's return type
1. the function's parameters type vector
1. the function's arity
1. a boolean telling if the function is variadic or not \(accepts a variable number of arguments\)


Finally, we can add the function we just defined to the module we defined earlier,
give it a name and get an `LLVMValueRef` as a result. This reference is the
concrete location of the function in memory.

#### Basic Blocks

A basic block represents a single entry or exit section of code, it contains a
list of instructions and it belongs to a function.
Those blocks only have an entry and exit point, meaning the list of instructions
will have to be executed from the beginning to the end. This means there cannot
be conditional loops or jumps of any kind inside those basic blocks. We can define
our basic block with the `LLVMAppendBasicBlock()` function as follows:

```language=c
LLVMBasicBlockRef entry = LLVMAppendBasicBlock(sum, "entry");
```


The `LLVMAppendBasicBlock` links the `entry` basic block to our previously defined
function. We now have a module that contains all the references to the function
we wanted to define and the different types and basic blocks it needed.

#### Instruction Builder

In order to add instructions to our function's unique basic block, we need to use
an instruction builder. This component, described in the documentation as
a point within a basic block and is the exclusive means of building instructions
using the C interface, is created using the `LLVMCreateBuilder()` function. Then, in the same way we added the basic block to the function,
we position the builder to start in the entry of the basic block using the `LLVMPositionBuilderAtEnd()` function.

```language=c
LLVMBuilderRef builder = LLVMCreateBuilder();
LLVMPositionBuilderAtEnd(builder, entry);
```


Now comes the function itself. We need to add the two arguments of our function
and return the result. In order to do this, we first have to retrieve the parameters
passed to the function. This can be done with the function `LLVMGetParam()` that
takes a reference to the function and the index of the desired parameter. Then,
we need to properly create an instruction to sum them up using `LLVMBuildAdd()` function, and providing a return holder
\(`tmp` here\). `LLVMBuildAdd` takes the two integers to add and a name to give
to the result. Note that this name is required due to LLVM's policy saying that
all instructions have to produce intermediate results

```language=c
LLVMValueRef tmp = LLVMBuildAdd(builder, LLVMGetParam(sum, 0), LLVMGetParam(sum, 1), "tmp");
```


Finally, we call the `LLVMBuildRet()` function to generate the return statement and arrange for the temporary result.

```language=c
LLVMBuildRet(builder, tmp);
```


#### Analysis


Our module is complete and we can check for any errors or exceptions by using the
tools present in the `analysis` library. For example, we can verify our module using the `LLVMVerifyModule` function.
This function will analyse our module and report for any errors:

```language=c
char *error = NULL;
LLVMVerifyModule(mod, LLVMAbortProcessAction, &error);
LLVMDisposeMessage(error);
```


### IR Generation

The LLVM IR is a strictly defined language that can be qualified as a
midway point between assembly and C. This LLVM IR can take three different forms
and aims at platform portability. Those three forms are the following:

- an in-memory set of objects, which we are using here
- a textual language like assembly
- a string of bytes binary encoded, called bitcode


We are currently using the first form but we can get the two others. Let's start
by writing the bitcode of the module to a file. In order to do so, we will use the
`bitwriter` library and its `LLVMWriteBitcodeToFile` function:

```language=c
if (LLVMWriteBitcodeToFile(mod, "sum.bc") != 0) {
    fprintf(stderr, "error writing bitcode to file, skipping\n");
}
```


In order to get this bitcode, we need to compile the C program as specified in
the makefile. If we look closer at the different commands we are executing, we
can find the following:

```language=bash
$ clang `llvm-config --cflags` -c sum.c
```


The first command compiles the C source code into an object file `sum.o`. We
need to provide the `--cflags` in order for LLVM to work correctly. The next
command will call the linker:

```language=bash
$ clang++ `llvm-config --cxxflags --ldflags --libs core analysis native bitwriter --system-libs` sum.o -o sum
```


Even though we are writing a C program, please remember that we are only using
the LLVM C bindings and API while LLVM itself is written in C++. That is why we
need the C++ linker in order for the generation of the executable to work.

Now in order to execute the output, simply perform:
```language=bash
$ ./sum
```


The bitcode will then be written to the file `sum.bc`. This bitcode is the second form of the IR.

![IR Bitcode](figures/bitcode_sum.png)

We can finally get the text output by using one of the LLVM tools, the disassembler.
This tool allows disassembling bitcode and outputting the IR textual representation.
It can be done by using `llvm-dis` as follows:

```language=bash
$ llvm-dis sum.bc
```


If we now look at the output `sum.ll`, we can see:

```language=llvm
; ModuleID = 'sum.bc'
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i32 @sum(i32, i32) {
entry:
  %tmp = add i32 %0, %1
  ret i32 %tmp
}
```


This is the LLVM representation of our `sum` function! Note that all the different
steps of the generation can be performed through the use of the makefile by doing:

```language=bash
$ make sum.o
$ make sum
$ make sum.bc
$ make sum.ll
```


Note that `make all` is the equivalent of `make sum`.

### References


The following links are useful pointers to the official LLVM reference API documentation we used in this chapter:

- [Modules](https://llvm.org/doxygen/group__LLVMCCoreModule.html)
- [Function Types](https://llvm.org/doxygen/group__LLVMCCoreTypeFunction.html)
- [Basic Blocks](https://llvm.org/doxygen/group__LLVMCCoreValueBasicBlock.html)
- [Instruction Builder](https://llvm.org/doxygen/group__LLVMCCoreInstructionBuilder.html)
- [Int Types](https://llvm.org/doxygen/group__LLVMCCoreTypeInt.html)
- [Analyses](https://llvm.org/doxygen/group__LLVMCAnalysis.html)
- [Bit Writer](https://llvm.org/doxygen/group__LLVMCBitWriter.html)

### Summary


In this chapter we learned how to build a function using the LLVM IR and create the bitcode
related to this IR. We explored the different types and
components that the C API provides and we created the function and its basic
block in a manual way. Ideally, what we just did will be scripted.

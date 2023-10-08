## Machine Code Generation


### `Triple`s, `Target`s and `TargetMachine`s

In the last chapter we generated the bitcode for the host machine. How can we
specify the machine we want to generate code for \(and by code I mean machine code\)?
This idea would use the main strength of LLVM, with one IR we can generate any
specified machine code. Now that we have the IR ready by creating it last chapter,
how can we specify the architecture we want to deploy it on?

In order to answer this issue, LLVM uses a `Target`/`TargetMachine` double
representation of any architecture and a `Triple` as a textual descriptor of
the architecture, vendor and operating system.

Basically, `Triple`s are helpers for configuration. Their names come from the
fact that they used to contain only three fields and are strings in the canonical
form of:
ARCHITECTURE - VENDOR - OPERATING SYSTEM \(- ENVIRONMENT\)

If some of the fields are not specified, the default ones will be used. The list
of all the architectures, vendors, operating systems and environments can be found
under: [Triple](https://llvm.org/doxygen/classllvm_1_1Triple.html#details).

The `Target` on the other hand is an object used only by LLVM under the hood
in order to profile the target you will be using and be used by the `TargetMachine`
object later on. These `Target` objects need to be initialised correctly before
being deduced from a `Triple`. They are used to describe that for which code may
be intended. They are used both during the lowering of LLVM IR to machine code and
by some optimisations passes.

The `TargetMachine` on the other hand takes a reference to a `Target` and then
uses a description of the machine through the specification of the triple and the cpu.
Last things provided to the `TargetMachine` are indicators on optimisations or
the type of model we want. Fortunately, those last bits are elements taken from
enumerations and will not be the source of any issue.

### Triple settings


The triple can either be obtained automatically from your actual machine by using
`LLVMGetDefaultTargetTriple()` or can be specified following the form specified
earlier. In our example, using `LLVMGetDefaultTargetTriple()` can be done as
follows:

```language=c
char* triple = LLVMGetDefaultTargetTriple(); // Using the triple of my machine
printf("%s\n",triple);
>>> x86_64-apple-darwin18.7.0
```


In order to get a coherent output, we will use a provided triple from now on:
```language=c
char triple[] = "x86_64";
char cpu[] = "";
```


The `cpu` string is used later on but can be defined by default by LLVM so we
will be using it this way. Basically, it will guess it based on the first part
of the triple string.

### Target Initialisation


Before being deduced from a `Triple`, any `Target` has to be initialised.
This initialisation can be specified case by case by using the different libraries
but since we want to be able to create machine code for any supported machine,
we need to use all the possible initialisations and use them all. These initialisations
are the four following:

```language=c
LLVMInitializeAllTargets();
LLVMInitializeAllTargetMCs();
LLVMInitializeAllTargetInfos();
LLVMInitializeAllAsmPrinters();
```


- [LLVMInitializeAllTargets\(\)](https://llvm.org/doxygen/group__LLVMCTarget.html#gace43bdd15ad030e38c41ae1cb43a2539)
- [LLVMInitializeAllTargetMCs\(\)](https://llvm.org/doxygen/group__LLVMCTarget.html#ga750b05e81ab9a921bb6d2e9b912e66eb)
- [LLVMInitializeAllTargetInfos\(\)](https://llvm.org/doxygen/group__LLVMCTarget.html#ga40188f383ddf8774ede38e8098da9a9a)
- [LLVMInitializeAllAsmPrinters\(\)](https://llvm.org/doxygen/group__LLVMCTarget.html#gadcbb41ca7051aca660022e70ee62dd7f)

1. The first initialisation `LLVMInitializeAllTargets()` enables the program to access all available targets that LLVM is configured to support.


1. The second initialisation `LLVMInitializeAllTargetMCs()` enables the program to access all available target Machine Codes.


1. The third initialisation `LLVMInitializeAllTargetInfos()` makes the program able to link all available targets that LLVM is configured to support.


1. The last initialisation `LLVMInitializeAllAsmPrinters()` gives access to all of the available ASM printers to the program.


Basically, **1** holds the description, **2** the machine code, **3** the linkers
and **4** the ASM printer.

Once the target initialisation is done, we can extract the `LLVMTargetRef` we want
from a given triple. This is done by creating an `LLVMTargetRef` that will be filled
by the `LLVMGetTargetFromTriple()` function. Adding an error pointer, we can get
the following piece of code:

```language=c
LLVMTargetRef targetRef;
char** errPtrTriple;
LLVMBool resTriple = LLVMGetTargetFromTriple(triple, &targetRef, errPtrTriple);
if (resTriple != 0)
{
    printf("%s\n",*errPtrTriple);
}
```

[LLVMGetTargetFromTriple\(\)](https://llvm.org/doxygen/c_2TargetMachine_8h.html#a7a746a65818e0b6bd86e5f00a568e301)

### Target Machine Initialisation


Now that we have a proper `Target` initialised, the next step is to create a
`TargetMachine`. This can be done by using the function `LLVMCreateTargetMachine()`.
However, this function takes seven arguments so let's take a closer look at how
we are going to use it:

```language=c
LLVMTargetMachineRef targetMachineRef = LLVMCreateTargetMachine(targetRef, triple, cpu, "", LLVMCodeGenLevelNone, LLVMRelocDefault, LLVMCodeModelDefault);
```


[LLVMCreateTargetMachine](https://llvm.org/doxygen/c_2TargetMachine_8h.html#a9b0b2b1efd30fad999f2b2a7fdbf8492)

The result is an `LLVMTargetMachineRef`, what we need and were expecting. The
seven arguments are the following:
- an `LLVMTargetRef`: The `Target` we defined earlier
- a string `Triple`: The `Triple` representing the target machine
- a string `cpu`: The cpu of the target machine
- a string `features`:
- an `LLVMCodeGenOptLevel`: The level of optimisation we want, this is an enumeration we will have to choose from \(`None`,`Less`,`Default` and `Agressive`\)
- an `LLVMRelocMode`: The mode of relocation we want, another enumeration we have to choose from \(`Default`,`Static`,`PIC`,etc.\)
- an `LLVMCodeModel`: This is a term from [AMD64 ABI](https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf) \(chapter 3.5\) that defines the offset between data and code within instructions, also an enumeration we can choose from \(`Default`,`JITDefault`,`Tiny`,`Small`,etc.\)


You can find more details on the existing elements of the enumerations by looking at
the following links in the documentation:
- [LLVMCodeGenOptLevel](https://llvm.org/doxygen/c_2TargetMachine_8h.html#acad7f73c0a2e7db5680d80becd5e719b)
- [LLVMRelocMode](https://llvm.org/doxygen/c_2TargetMachine_8h.html#ac6ed8c89bb69e7a56474cac6cf0ffb67)
- [LLVMCodeModel](https://llvm.org/doxygen/c_2TargetMachine_8h.html#a333ec2da299d964c0885bee025bef68c)


### Emission to file


Finally, once the `TargetMachine` is correctly created, we can emit the module
either to an object or an ASM file. This is done through the function `LLVMTargetMachineEmitToFile()`.

The function takes several arguments, from which:
- the `TargetMachine` reference,
- the module,
- a name for the new file \(we specified `.o` here for the object file\),
- the type of file, either `LLVMObjectFile` or `LLVMAssemblyFile` \(both are taken from the `LLVMCodeGenFileType` enumeration\)
- an error pointer


In the end, the result looks like:

```language=c
char** errPtrFileObj;
LLVMBool resFileObj = LLVMTargetMachineEmitToFile(targetMachineRef, mod, "sum_llvm.o", LLVMObjectFile, errPtrFileObj);
if (resFileObj != 0)
{
    printf("%s\n",*errPtrFileObj);
}
```


or

```language=c
char** errPtrFileAsm;
LLVMBool resFileAsm = LLVMTargetMachineEmitToFile(targetMachineRef, mod, "sum_llvm.asm", LLVMAssemblyFile, errPtrFileAsm);
if (resFileAsm != 0)
{
    printf("%s\n",*errPtrFileAsm);
}
```


In order to get the output, run the provided makefile. It differs from the first
one as it needs all the libraries to run the target initialisation. The output
will generate a `.o` and `.asm` file. If we take a look at the `.asm` file
we can see:

```language=asm
.section	__TEXT,__text,regular,pure_instructions
.macosx_version_min 10, 14
.globl	_sum
.p2align	4, 0x90
_sum:
.cfi_startproc
addl	%esi, %edi
movl	%edi, %eax
retq
.cfi_endproc


.subsections_via_symbols
```


### Summary

What did we learn in this chapter?
We extended the function we defined in the previous chapter by now being able
to state and select a specific architecture to target and aim for when building
the machine code. Those steps lead to a file \(either object or ASM\). When creating
the Pharo bindings corresponding to the LLVM-C interface, we will want to emit
the machine code to a memory buffer rather than a file and this can be done the
exact same way we did while emitting to a file but using the function
`LLVMTargetMachineEmitToMemoryBuffer()`

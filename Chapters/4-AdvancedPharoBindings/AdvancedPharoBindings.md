## Advanced Pharo Bindings


### Development plan

Following the previous chapter, we will continue our retranscription of the C
code we managed to produce by the end of Chapter 2. This code allowed us in the
end to obtain a file with the module we defined translated to machine code of
a given architecture. In order to get to the same point from within Pharo, the
last steps we need to do are the following:

- Memory buffer and emission
- Target creation
- Enumerations definition
- Target Machine creation


### Memory Buffer and Emission

Following the same idea of what we did by the end of Chapter 2, we will need a
way to output the module bitcode. This time, we do not need to direct it to a
file but we would like to keep the representation inside the Pharo environment.
We can do this by emitting the module bitcode to a memory buffer instead of a
file. Therefore, let's first define the `LLVMMemoryBuffer`:

```language=Pharo
FFIExternalObject subclass: #LLVMMemoryBuffer
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


Then, we can add the method transcripting the `LLVMWriteBitcodeToMemoryBuffer()`
\([LLVMWriteBitcodeToMemoryBuffer](https://llvm.org/doxygen/group__LLVMCBitWriter.html#ga43cccd6ab4fe5c042fc59d972430c97f)\).
Adding this method will make more sense in the `LLVMModule` class and can be
done as follows:

```language=Pharo
LLVMModule >> emitBitCodeToMemoryBuffer
	^ self ffiCall: #(LLVMMemoryBuffer LLVMWriteBitcodeToMemoryBuffer(LLVMModule self))
```


We can now use the piece of code from the summary of Chapter 3 and add the
emission to the memory buffer to get:

```language=Pharo
| mod paramArray sumSig retType sum builder param1 param2 tmp memBuff |

mod := LLVMModule withName: 'mod'.
paramArray := LLVMParameterArray withSize: 2.
paramArray at: 1 put: (LLVMInt32 new handle getHandle).
paramArray at: 2 put: (LLVMInt32 new handle getHandle).
paramArray.

retType := LLVMInt32 new.
sumSig := LLVMFunctionSignature withReturnType: retType parametersVector: paramArray arity: 2 andIsVaridic: false.

sum := sumSig addToModule: mod withName: 'sum'.

builder := LLVMBuilder new.

param1 := sum parameterAtIndex: 1.
param2 := sum parameterAtIndex: 2.

tmp := builder buildAdd: param1 to: param2 andStoreUnder: 'tmp'.
builder buildReturnStatementFromValue: tmp.

memBuff := mod emitBitCodeToMemoryBuffer.

>>> a LLVMMemoryBuffer((void*)@ 16r7F878F4A46B0)
```


This corresponds to what we managed to get by the end of Chapter 1, except it is
now inside a memory buffer rather than written to a file.

### Target creation

The `LLVMTarget` class will be tricky. If you remember correctly, we created
the `Target` from a `Triple` string and we did that thanks to the function
`LLVMGetTargetFromTriple()` \([LLVMGetTargetFromTriple](https://llvm.org/doxygen/c_2TargetMachine_8h.html#a7a746a65818e0b6bd86e5f00a568e301)\).
What is new compared to the other classes? The function we would like to use
does not return an `LLVMTarget` but gets as argument a pointer to an `LLVMTarget`
that will be filled with the resulting target. Moreover, this function takes a
string pointer \(`char**`\) to write the error if one appears.

In order to define pointers to objects from within Pharo, we have to define
`FFIExternalValueHolder`s as class variables. They can then be initialised from
within an  `initialize` class method:

```language=Pharo
FFIExternalObject subclass: #LLVMTarget
	instanceVariableNames: ''
	classVariableNames: 'LLVMTarget_PTR String_PTR'
	package: 'LLVMBindings-Target'
```


```language=Pharo
LLVMTarget class >> initialize
    LLVMTarget_PTR := FFIExternalValueHolder ofType: 'LLVMTarget'.
    String_PTR := FFIExternalValueHolder ofType: 'String'.
```


We can then call the function `LLVMGetTargetFromTriple` by using it as follows
and calling our class variables:

```language=Pharo
LLVMTarget class >> getTargetIn: targetHolder fromTriple: triple errorMessage: errorHolder
    ^ self ffiCall: #(Boolean LLVMGetTargetFromTriple(const char* triple,
                                                      LLVMTarget_PTR targetHolder,
                                                      String_PTR errorHolder))
```


Then, a nice thing to do is to write a wrapper around the above method in order
to get a coherent output in case of an error:

```language=Pharo
getTargetFromTriple: triple
    | targetHolder errorHolder hasError |

    targetHolder := LLVMTarget_PTR new.
    errorHolder := String_PTR new.

    "Returns 0 (false) on success"
    hasError := LLVMTarget getTargetIn: targetHolder fromTriple: triple errorMessage: errorHolder.
    hasError ifTrue: [ self error: errorHolder value ].

    ^ LLVMTarget fromHandle: targetHolder value.
```


There it is, we can define our target right? If you remember correctly from Chapter
2, one of the necessary step was to tell LLVM which targets we wanted to initialise
before being able to actually use those targets. We got around this issue by using
the functions `LLVMInitializeAllTargets`, `LLVMInitializeAllTargetInfos`,
`LLVMInitializeAllAsmPrinters` and `LLVMInitializeAllTargetMCs`. However,
those functions are defined as macros inside the C headers. This means that from
C, they will be recognised as long as the header is included due to the fact that
they will be processed by the C preprocessor. However, the only way to use them
from Pharo would be to replicate their behaviour... Fortunately, what we will be
able to do is to initialise the targets one by one. We will do this for one of
them but can be replicated.

```language=Pharo
LLVMTarget class >> initializeX86Target
	self ffiCall: #(void LLVMInitializeX86Target())
```


```language=Pharo
LLVMTarget class >> initializeX86TargetInfo
	self ffiCall: #(void LLVMInitializeX86TargetInfo())
```


```language=Pharo
LLVMTarget class >> initializeX86TargetMC
	self ffiCall: #(void LLVMInitializeX86TargetMC())
```


```language=Pharo
LLVMTarget class >> initializeX86AsmPrinter
	self ffiCall: #(void LLVMInitializeX86AsmPrinter())
```


And finally, a gatherer of all the above methods:

```language=Pharo
LLVMTarget class >> initializeX86
	self initialize.
	self initializeX86Target.
	self initializeX86TargetInfo.
	self initializeX86TargetMC.
	self initializeX86AsmPrinter.
```


The exact same methods can defined for the architectures `AArch64` or `ARM`
for example. We can now add some helper functions to interact with an `LLVMTarget`
object:

```language=Pharo
LLVMTarget >> description
	^ self ffiCall: #(const char *LLVMGetTargetDescription(LLVMTarget self))
```


```language=Pharo
LLVMTarget >> name
	^ self ffiCall: #(const char *LLVMGetTargetName(LLVMTarget self))
```


### Enumerations definition

Our target is set and ready. However, before being able to create a `TargetMachine`,
we will need to define some enumerations. Enumerations are finite sets of data-types
that can be used as a type later on. Here, when we will have to define a `TargetMachine`,
we will have to choose an option from three different enumerations:

- `LLVMCodeGenOptLevel`: The level of optimisation we want, this is an enumeration we will have to choose from \(`None`,`Less`,`Default` and `Agressive`\)
- `LLVMRelocMode`: The mode of relocation we want, another enumeration we have to choose from \(`Default`,`Static`,`PIC`,etc.\)
- `LLVMCodeModel`: This is a term from [AMD64 ABI](https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf) \(chapter 3.5\) that defines the offset between data and code within instructions, also an enumeration we can choose from \(`Default`,`JITDefault`,`Tiny`,`Small`,etc.\)


We will go through the creation of the `LLVMCodeGenOptLevel` enumeration. First,
let's define the class as a subclass of `FFIEnumeration`:

```language=Pharo
FFIEnumeration subclass: #LLVMCodeGenOptLevel
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Enumeration'
```


Then, we will need to create a class method `enumDecl` that holds the different
values:

```language=Pharo
LLVMCodeGenOptLevel class >> enumDecl
	^ #(
	llvmCodeGenLevelNone       0
	llvmCodeGenLevelLess       1
	llvmCodeGenLevelDefault    2
	llvmCodeGenLevelAggressive 3
	)
```


Once this is done, run the `LLVMCodeGenOptLevel initialize` in order to properly
create the enumeration. If everything went right, you should now have the
following class definition:

```language=Pharo
FFIEnumeration subclass: #LLVMCodeGenOptLevel
	instanceVariableNames: ''
	classVariableNames: 'llvmCodeGenLevelAggressive llvmCodeGenLevelDefault llvmCodeGenLevelLess llvmCodeGenLevelNone'
	package: 'LLVMBindings-Enumeration'
```


The same thing can be done for the two other enumerations, here are the results:

- `LLCMRelocMode`

```language=Pharo
FFIEnumeration subclass: #LLVMRelocMode
	instanceVariableNames: ''
	classVariableNames: 'llvmRelocDefault llvmRelocDynamicNoPic llvmRelocPIC llvmRelocROPI llvmRelocROPI_RWPI llvmRelocRWPI llvmRelocStatic'
	package: 'LLVMBindings-Enumeration'
```


```language=Pharo
LLVMRelocMode class >> enumDecl
	^ #(
	llvmRelocDefault      0
	llvmRelocStatic       1
	llvmRelocPIC          2
	llvmRelocDynamicNoPic 3
	llvmRelocROPI         4
	llvmRelocRWPI         5
	llvmRelocROPI_RWPI    6
	)
```


- `LLVMCodeModel`

```language=Pharo
FFIEnumeration subclass: #LLVMCodeModel
	instanceVariableNames: ''
	classVariableNames: 'llvmCodeModelDefault llvmCodeModelJITDefault llvmCodeModelKernel llvmCodeModelLarge llvmCodeModelMedium llvmCodeModelSmall llvmCodeModelTiny llvmodeModelDefault'
	package: 'LLVMBindings-Enumeration'
```


```language=Pharo
LLCMCodeModel class >> enumDecl
	^ #(
	llvmodeModelDefault     0
	llvmCodeModelJITDefault 1
	llvmCodeModelTiny       2
	llvmCodeModelSmall      3
	llvmCodeModelKernel     4
  llvmCodeModelMedium     5
	llvmCodeModelLarge      6
	)
```


### Target Machine Creation

We now have the enumerations we will need in order to properly create a
`TargetMachine`. In order to make those enumerations available to the class,
we need to add them as `poolDictionaries`. The resulting class definition is
the following:

```language=Pharo
FFIExternalObject subclass: #LLVMTargetMachine
	instanceVariableNames: ''
	classVariableNames: ''
	poolDictionaries: 'LLVMCodeGenOptLevel LLVMRelocMode LLVMCodeModel'
	package: 'LLVMBindings-Target'
```


The function `LLVMCreateTargetMachine()`
\([LLVMCreateTargetMachine](https://llvm.org/doxygen/c_2TargetMachine_8h.html#a9b0b2b1efd30fad999f2b2a7fdbf8492)\)
can be written as follows:

```language=Pharo
LLVMTargetMachine class >> fromTarget: aTarget withTriple: aTripleString withCPU: aCPUString withFeatures: aFeaturesString withOptLevel: anOptimizationLevel withRelocMode: aRelocMode andCodeModel: aCodeModel
	^ self ffiCall: #(LLVMTargetMachine LLVMCreateTargetMachine(LLVMTarget aTarget,
                                  String 	              aTripleString,
                                  String 	              aCPUString,
                                  String 	              aFeaturesString,
                                  LLVMCodeGenOptLevel  anOptimizationLevel,
                                  LLVMRelocMode        aRelocMode,
                                  LLVMCodeModel 	     aCodeModel))
```


We will definitely not want to have to call this function with its seven different
arguments. Let's add a default version wrapper:

```language=Pharo
LLVMTargetMachine class >> fromTarget: aTarget withTriple: aTripleString
	^ LLVMTargetMachine fromTarget: aTarget withTriple: aTripleString withCPU: '' withFeatures: '' withOptLevel: llvmCodeGenLevelDefault withRelocMode: llvmRelocDefault andCodeModel: llvmCodeModelDefault
```


As you can see, the elements of the enumerations can be called simply by using
them now that we have added the enumerations as used pool dictionaries. Going to
the instance side, we will add some helpers as well as the classic `dispose` and
`finalize` combo:

```language=Pharo
LLVMTargetMachine >> triple
	^ self ffiCall: #(String LLVMGetTargetMachineTriple(LLVMTargetMachine self))
```


```language=Pharo
LLVMTargetMachine >> target
	^ self ffiCall: #(LLVMTarget LLVMGetTargetMachineTarget(LLVMTargetMachine self))
```


```language=Pharo
LLVMTargetMachine >> cpu
	^ self ffiCall: #(String LLVMGetTargetMachineCPU(LLVMTargetMachine self))
```


```language=Pharo
LLVMTargetMachine >> featureString
	^ self ffiCall: #(String LLVMGetTargetMachineFeatureString(LLVMTargetMachine self))
```


```language=Pharo
LLVMTargetMachine >> dispose
	self ffiCall: #(void LLVMDisposeTargetMachine(LLVMTargetMachine self))
```


```language=Pharo
LLVMTargetMachine >> finalize
	self dispose
```



### Memory Buffer Emission

The last step to cover the Pharo equivalent of Chapter 2 is to write the last
binding for the emission to a memory buffer. First, we will need a last enumeration
that will let us decide to what type of file we will like to output the code,
having the choice between an object or assembly file.

```language=Pharo
FFIEnumeration subclass: #LLVMCodeGenFileType
	instanceVariableNames: ''
	classVariableNames: 'llvmAssemblyFile llvmObjectFile'
	package: 'LLVMBindings-Enumeration'
```


```language=Pharo
LLVMCodeGenFileType class >> enumDecl
	^ #(
	llvmAssemblyFile 0
	llvmObjectFile   1
	)
```


Now, the final method we want to add is `LLVMTargetMachineEmitToMemoryBuffer()`
\([LLVMTargetMachineEmitToMemoryBuffer](https://llvm.org/doxygen/c_2TargetMachine_8h.html#aaa9ce583969eb8754512e70ec4b80061)\).
This function needs two pointers that we will have to implement the same way we
did above with `getTargetFromTriple`. Let's rewrite the `LLVMModule` a bit:

```language=Pharo
FFIExternalObject subclass: #LLVMModule
	instanceVariableNames: ''
	classVariableNames: 'LLVMMemBuffer_PTR String_PTR'
	poolDictionaries: 'LLVMCodeGenFileType'
	package: 'LLVMBindings-Core'
```


We need to add the `LLVMCodeGenFileType` enumeration to the list of `poolDictionaries`.
Moreover, you can see we defined two new class variables `LLVMMemBuff_PTR` and
`String_PTR` \(our error pointer\).

```language=Pharo
LLVMModule class >> initialize
	String_PTR := FFIExternalValueHolder ofType: 'String'.
	LLVMMemBuffer_PTR := FFIExternalValueHolder ofType: 'LLVMMemoryBuffer'.
```


We will have to call the `initialize` in our module creation method, `withName:`:

```language=Pharo
LLVMModule class >> withName: aName
	self initialize.
	^ self ffiCall: #(LLVMModule LLVMModuleCreateWithName(String aName))
```


Now we can finally write the method to emit the module to a memory buffer given
a proper target machine:

```language=Pharo
LLVMModule >> emitCodeFromTargetMachine: aTargetMachine toMemoryBuffer: aMemBufferPtr withFileType: aCodeFileType withError: anErrorHolder
	^ self ffiCall: #(Boolean LLVMTargetMachineEmitToMemoryBuffer(LLVMTargetMachine aTargetMachine,
                                           LLVMModule self,
                                           LLVMCodeGenFileType aCodeFileType,
                                           String_PTR anErrorHolder,
                                           LLVMMemBuffer_PTR aMemBufferPtr))
```


Once again, a wrapper to correctly display the error if it is met is nice:

```language=Pharo
LLVMModule >> emitCodeFromTargetMachine: aTargetMachine withFileType: aCodeFileType

	| memBufferHolder errorHolder haveError |

    memBufferHolder := LLVMMemBuffer_PTR new.
    errorHolder := String_PTR new.

  	"Returns 0 (false) on success"
    hasError := self emitCodeFromTargetMachine: aTargetMachine toMemoryBuffer: memBufferHolder withFileType: aCodeFileType withError: errorHolder.
    hasError ifTrue: [ self error: errorHolder value ].

    ^ LLVMMemoryBuffer fromHandle: memBufferHolder value.
```



Finally, we can write the last functions for each of the file type we know:

```language=Pharo
LLVMModule >> emitObjFromTargetMachine: aTargetMachine
	^ self emitCodeFromTargetMachine: aTargetMachine withFileType: llvmObjectFile
```


```language=Pharo
LLVMModule >> emitASMFromTargetMachine: aTargetMachine
	^ self emitCodeFromTargetMachine: aTargetMachine withFileType: llvmAssemblyFile
```


### Summary

We covered in this chapter the last more advanced bindings in order to get the
same behaviour as the one we were getting from C. Given our bindings, we should
be able to make the following piece of code work:

```language=Pharo
| mod paramArray sumSig retType sum builder param1 param2 tmp memBuff  target targetMachine |

"MODULE DEFINITION"
mod := LLVMModule withName: 'mod'.

"PARAMETERS ARRAY DEFINITION"
paramArray := LLVMParameterArray withSize: 2.
paramArray at: 1 put: (LLVMInt32 new handle getHandle).
paramArray at: 2 put: (LLVMInt32 new handle getHandle).
paramArray.

"FUNCTION SIGNATURE DEFINITION"
retType := LLVMInt32 new.
sumSig := LLVMFunctionSignature withReturnType: retType parametersVector: paramArray arity: 2 andIsVaridic: false.

"FUNCTION DEFINITION"
sum := sumSig addToModule: mod withName: 'sum'.

"BUILDER DEFINITION AND OPERATIONS"
builder := LLVMBuilder new.

param1 := sum parameterAtIndex: 1.
param2 := sum parameterAtIndex: 2.

tmp := builder buildAdd: param1 to: param2 andStoreUnder: 'tmp'.
builder buildReturnStatementFromValue: tmp.

"TARGET DEFINITION"
LLVMTarget initializeX86.
target := LLVMTarget getTargetFromTriple: 'x86_64'.

"TARGET MACHINE DEFINITION"
targetMachine := LLVMTargetMachine fromTarget: target withTriple: 'x86_64'.

memBuff := mod emitASMFromTargetMachine: targetMachine.
```

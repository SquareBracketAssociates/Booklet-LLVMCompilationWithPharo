## Pharo LLVM Bindings


### How can we use LLVM from Pharo?

In order to use LLVM from Pharo, the main solution is to call the LLVM C bindings
and use those with the help of the uFFI package \(Unified Foreign Function Interface\).
This package is a façade to all external libraries that one might want to use
from within Pharo. C libraries can therefore be called and used.

Even though Pharo and C are extremely different, the calls can operate by marshalling
correctly the different types, structures and by binding the different functions
with respect to some rules. In order to use the Pharo uFFI library, we will go
over the piece of code we wrote in the first chapter and try to make it work from
within a Pharo image.

### Development plan

The uFFI library is best used either as a façade design pattern where one object
will hold all of the different bindings that are present in the library we want
to use. This way works well if the library you are writing a binding for is
small and can be contained within a few functions.
Another way to better transcribe your library that is more suitable for 
important libraries is to replicate an object-oriented behaviour for the different
structures or objects you are dealing with. As the LLVM library is wide and might
keep on expanding, we will choose the second option.

By looking back at the final code from the first chapter, the different steps we
will be aiming for are:
- Module definition
- Parameters vector creation
- Function definition
- Basic block creation
- Builder creation and operations
- Output to a memory buffer


### Module definition

The first element and top container of LLVM is a `Module`. Let's define the
class within a freshly created package:

```language=Pharo
FFIExternalObject subclass: #LLVMModule
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


The first methods we will want to define in this class \(but also in all the other
classes using external bindings\) are `ffiLibrary`:

```language=Pharo
LLVMModule >> ffiLibrary
	^ self class ffiLibrary
```


```language=Pharo
LLVMModule class >> ffiLibrary
	^ 'libLLVM.dylib' asFFILibrary
```


Those two methods indicate to the class where to look for the library we want to
call. In our case, the `libLLVM.dylib` is the place to go. Note that this name
can differ depending on the platform you are using \(e.g. `*.so` on Linux or
`*.dll` on Windows\).

Now, the function we want to use is `LLVMCreateModuleWithName()`
\([LLVMModuleCreateWithName\(\)](https://llvm.org/doxygen/group__LLVMCCoreModule.html#ga8cf6711b9359fb55d081bfc5e664370c)\)
In order to output an `LLVMModule`, we can define the binding as a class function
that could allow us to create a module as follows:

```language=Pharo
LLVMModule withName: 'aModule'.
>>> LLVMModule
```


This can be done by defining the binding on the class side of `LLVMModule`:
```language=Pharo
LLVMModule class >> withName: aName
	^ self ffiCall: #(LLVMModule LLVMModuleCreateWithName(String aName))
```


Now that our module creation is working, we can add some other functions that
are used on modules from within LLVM. Note that this can be done easily due to
the choice of the bindings structure we made. Adding a function while using the
façade design pattern would make us take a reference to the module as well. Here,
we can just use any function that needs a module as an argument and pass our
object to it! For example, the function `LLVMGetTarget()` \([LLVMGetTarget](https://llvm.org/doxygen/group__LLVMCCoreModule.html#ga25a0be3489b6c0f0b517828d84e376f3)\)
that outputs the `Triple` representing the target of a given module and which
signature is:

```language=c
const char* LLVMGetTarget	(	LLVMModuleRef 	M	)
```


Can be written in Pharo as:
```language=Pharo
LLVMModule >> target
	self ffiCall: #(const char *LLVMGetTarget(LLVMModule self))
```


A last thing we need to do for our module to be fully operational \(for now\) is to
write its `dispose` function. In a C world, this would be required when the object
is no longer in use \(in a way `free` would be used\). In Pharo, we can encapsulate
this behaviour in the `finalize` method that is called automatically while
garbage collecting a Pharo object. We would like to call `LLVMDisposeModule*()`
\([LLVMDiaposeModule](https://llvm.org/doxygen/group__LLVMCCoreModule.html#ga4acadb366d5ff0405371508ca741a5bf)\)
during this phase. This can be done by defining the two methods:

```language=Pharo
LLVMModule >> finalize
	self dispose
```


```language=Pharo
LLVMModule >> dispose
	self ffiCall: #(void LLVMDisposeModule(LLVMModule self))
```



### Parameters vector creation

Now that our module is done, we need to get the parameters array. It consists of
the types of the signature of a function. In our example:

```language=c
LLVMTypeRef param_types[] = { LLVMInt32Type(), LLVMInt32Type() };
```


In order to replicate this behaviour we need to define what an `LLVMInt32Type`
is and to see how to create an external array in Pharo. First, we can define a
superclass `LLVMType` from which our `LLVMInt32` will inherit. This is done
as follows:

```language=Pharo
FFIExternalObject subclass: #LLVMType
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


```language=Pharo
LLVMType subclass: #LLVMInt32
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


Do not forget to define the `ffiLibrary` class and instance methods under
`LLVMType` as it is needed in any class using bindings! Now, to create a proper
`LLVMInt32`, we can override the `new` method as follows:

```language=Pharo
LLVMInt32 class >> new
	^ self ffiCall: #(LLVMInt32 LLVMInt32Type(void))
```


In order to create an array and populate it with the correct type, we need to use
the uFFI featured object `FFIArray`. It can be defined and created as follows:

```language=Pharo
FFIArray subclass: #LLVMParameterArray
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


```language=Pharo
LLVMParameterArray class >> withSize: aNumber
	^  FFIArray newType: LLVMType size: aNumber
```


In the end, our array can be created and populated as shown below:
```language=Pharo
paramArray := LLVMParameterArray withSize: 2.
paramArray at: 1 put: (LLVMInt32 new handle getHandle).
paramArray at: 2 put: (LLVMInt32 new handle getHandle).
```


### Function definition

An important aspect of the functions as they are defined in LLVM is that they are
both represented under a `Type` and a `Value`. The `Type` represents the
function signature and adding the signature to a module will output a `Value`.
This `Value` is the location of the function in memory. `Value`s are used in
LLVM to represent many different objects such as function parameters, instructions
or basically any object that has an in-memory representation.

First things first, let's define a new `LLVMType`, `LLVMFunctionSignature`:

```language=Pharo
LLVMType subclass: #LLVMFunctionSignature
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


From within this class, the method that can instantiate it is `LLVMFunctionType()`
\([LLVMFunctionType](https://llvm.org/doxygen/group__LLVMCCoreTypeFunction.html#ga8b0c32e7322e5c6c1bf7eb95b0961707)\)
that we can bind as follows:

```language=Pharo
LLVMFunctionSignature class >> withReturnType: aType parametersVector: anArray arity: anInteger andIsVaridic: aBoolean
^ self ffiCall: #(#LLVMFunctionSignature LLVMFunctionType(LLVMType aType,
															            LLVMParameterArray anArray,
															            int anInteger,
															            Boolean aBoolean))
```


Now, we can populate the instance side with some accessors that might come in
handy:


```language=Pharo
LLVMFunctionSignature >> returnType
	^ self ffiCall: #(LLVMType LLVMGetReturnType(LLVMFunctionSignature self))
```

[LLVMGetReturnType\(\)](https://llvm.org/doxygen/group__LLVMCCoreTypeFunction.html#gacfa4594cbff421733add602a413cae9f)

```language=Pharo
LLVMFunctionSignature >> parametersNumber
	^ self ffiCall: #(uint LLVMCountParamTypes(LLVMFunctionSignature self))
```

[LLVMCountParamTypes\(\)](https://llvm.org/doxygen/group__LLVMCCoreTypeFunction.html#ga44fa41d22ed1f589b8202272f54aad77)

```language=Pharo
LLVMFunctionSignature >> isVaridic
	^ self ffiCall: #(Boolean LLVMIsFunctionVarArg(LLVMFunctionSignature self))
```

[LLVMIsFunctionVarArg\(\)](https://llvm.org/doxygen/group__LLVMCCoreTypeFunction.html#ga2970f0f4d9ee8a0f811f762fb2fa7f82)

Finally, we can add the function to the module and get the output `Value`. In
order to do so, let's first define an `LLVMValue` and a subclass `LLVMFunction`.

```language=Pharo
FFIExternalObject subclass: #LLVMValue
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


```language=Pharo
LLVMValue subclass: #LLVMFunction
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


Then, the function `LLVMAddFunction()` \([LLVMAddFunction](https://llvm.org/doxygen/group__LLVMCCoreModule.html#ga12f35adec814eb1d3e9a2090b14f74f5)\)
can be defined in the `LLVMFunctionSignature` as:

```language=Pharo
LLVMFunctionSignature >> addToModule: aModule withName: aName
	^ self ffiCall: #(LLVMFunction LLVMAddFunction(LLVMModule aModule,
	                                               String aName,
	                                               LLVMFunctionSignature self))
```


Some accessors can be defined as well in the `LLVMFunction` class, from which
I chose `LLVMGetFirstParam()`, `LLVMGetLastParam()`, `LLVMGetParam()`, ...
that I will showcase below. Those accessors use another subclass of `LLVMValue`,
an `LLVMFunctionParameter`.

```language=Pharo
LLVMValue subclass: #LLVMFunctionParameter
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


```language=Pharo
LLVMFunction >> firstParameter
	^ self ffiCall: #(LLVMFunctionParameter LLVMGetFirstParam(LLVMFunction self))
```


```language=Pharo
LLVMFunction >> lastParameter
	^ self ffiCall: #(LLVMFunctionParameter LLVMGetLastParam(LLVMFunction self))
```


```language=Pharo
LLVMFunction >> nextParameterOf: aParameter
	^ self ffiCall: #(LLVMFunctionParameter LLVMGetNextParam(LLVMFunction self))
```


```language=Pharo
LLVMFunction >> parameterAtIndex: anInteger
	^ self ffiCall: #(LLVMFunctionParameter LLVMGetParam(LLVMFunction self, uint anInteger))
```


```language=Pharo
LLVMFunction >> parametersNumber
	^ self ffiCall: #(uint LLVMCountParams(LLVMFunction self))
```


```language=Pharo
LLVMFunction >> previousParameterOf: aParameter
	^ self ffiCall: #(LLVMFunctionParameter LLVMGetPreviousParam(LLVMFunction self))
```



### Basic block creation

Basic block can be created from a function by appending them to it with the help
of the `LLVMAppendBasicBlock()` function. This function will correspond to the
way we will be instantiating our `LLVMBasicBlock`.

```language=Pharo
FFIExternalObject subclass: #LLVMBasicBlock
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


```language=Pharo
LLVMBasicBlock >> linkToFunction: aFunctionValue withName: aName
	^ self ffiCall: #(LLVMBasicBlock LLVMAppendBasicBlock(LLVMValue aFunctionValue,
                                                        String aName))
```

[LLVMAppendBasicBlock](https://llvm.org/doxygen/group__LLVMCCoreValueBasicBlock.html#ga74f2ff28344ef72a8206b9c5925be543)

And we can now add some helper functions such as:

```language=Pharo
LLVMBasicBlock >> name
	^ self ffiCall: #(const char* LLVMGetBasicBlockName(LLVMBasicBlock self))
```


```language=Pharo
LLVMBasicBlock >> parent
	^ self ffiCall: #(LLVMValue LLVMGetBasicBlockParent(LLVMBasicBlock self))
```


```language=Pharo
LLVMBasicBlock >> delete
	self ffiCall: #(void LLVMDeleteBasicBlock(LLVMBasicBlock self))
```


### Builder Creation and Operations

The builder is created through the `LLVMCreateBuilder()` function
\([LLVMCreateBuilder](https://llvm.org/doxygen/group__LLVMCCoreInstructionBuilder.html#gaceec9933fd90a94ea5ebb40eedf6136d)\).
This function will help us initialise our builder by creating the following class
and method:

```language=Pharo
FFIExternalObject subclass: #LLVMBuilder
	instanceVariableNames: ''
	classVariableNames: ''
	package: 'LLVMBindings-Core'
```


```language=Pharo
LLVMBuilder class >> new
	^ self ffiCall: #(LLVMBuilder LLVMCreateBuilder(void))
```


A similar call to `finalize` and `dispose` can be done:

```language=Pharo
LLVMBuilder >> dispose
	self ffiCall: #(void LLVMDisposeBuilder(LLVMBuilder self))
```


```language=Pharo
LLVMBuilder >> finalize
	^ self dispose
```


Now, we need to define the actual builder operations \(we will simply define the
add operation as well as the building of the return value\) and positioning. In
order to define the `LLVMBuildAdd` function
\([LLVMBuildAdd](https://llvm.org/doxygen/group__LLVMCCoreInstructionBuilder.html#ga5e20ba4e932d72d97a69e07ff54cfa81)\),
we proceed as follows:

```language=Pharo
LLVMBuilder >> buildAdd: aValue to: anotherValue andStoreUnder: aTemporaryValueName
	^ self ffiCall: #(LLVMValue LLVMBuildAdd(LLVMBuilder self,
                                            LLVMValue 	 aValue,
                                            LLVMValue 	 anotherValue,
                                            const char * aTemporaryValueName ))
```


Next, `LLVMBuildRet` \([LLVMBuildRet](https://llvm.org/doxygen/group__LLVMCCoreInstructionBuilder.html#gae4c870d69f9787fe98a824a634473155)\):

```language=Pharo
LLVMBuilder >> buildReturnStatementFromValue: aValue
	^ self ffiCall: #(LLVMValue LLVMBuildRet(LLVMBuilder self,
                                           LLVMValue aValue))
```


Finally, we will need to position our builder at the end of a basic block. We will
define some more positioning functions as well. The first one, and the one we will
use is `LLVMPositionBuilderAtEnd()` \([LLVMPositionBuilderAtEnd](https://llvm.org/doxygen/group__LLVMCCoreInstructionBuilder.html#gafa58ecb369fc661ff7e58c19c46053f0)\):

```language=Pharo
LLVMBuilder >> positionBuilderAtEndOfBasicBlock: aBasicBlock
	^ self ffiCall: #(void LLVMPositionBuilderAtEnd(LLVMBuilder self,
														   		  LLVMBasicBlock aBasicBlock))
```


Next come `LLVMPositionBuilderBefore()` \([LLVMPositionBuilderBefore](https://llvm.org/doxygen/group__LLVMCCoreInstructionBuilder.html#gaed4e5d25ea3f3a3554f3bc4a4e22155a)\)
and `LLVMPositionBuilder()` \([LLVMPositionBuilder](https://llvm.org/doxygen/group__LLVMCCoreInstructionBuilder.html#gaf8bc5c4564732d52c5aaae2cf56d2f5c)\).
Those two functions are used to position the builder on a particular instruction.
We can implement them as follows:

```language=Pharo
LLVMBuilder >> positionBuilderBeforeInstruction: anInstruction
	^ self ffiCall: #(void LLVMPositionBuilderBefore(LLVMBuilder self,
                                                   LLVMInstruction anInstruction))
```


```language=Pharo
LLVMBuilder >> positionBuilderOnInstruction: anInstruction inBasicBlock: aBasicBlock
	^ self ffiCall: #(void LLVMPositionBuilder(LLVMBuilder     self,
                                             LLVMBasicBlock  aBasicBlock,
                                             LLVMInstruction anInstruction))
```


The builder is now complete! By adding a small method to the `LLVMModule` to
emit the code to a memory buffer, we will have covered the code used in Chapter
1 but from within Pharo! We will add this method in the next chapter, when
defining the class `LLVMMemoryBuffer`.

### Summary

In this first part on Pharo bindings, we defined the basic bindings of the LLVM
library. This is the beginning but some of the choices needed to be explained.
Using a more object-oriented structure rather than a façade helps adding helper
function or scale the binding library in the future, makes it easier to navigate
through code and brings more familiarity from the Pharo environment within the
LLVM world. For now, we can run the following piece of code and expect it to work:

```language=Pharo
| mod paramArray sumSig retType sum builder param1 param2 tmp |

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

>>> a LLVMValue((void*)@ 16r7F878F285658)
```


In the next chapter, we will focus on the memory buffer and the retranscription
of the target and target machine. These are more complicated elements and they will
need a bit more work in order to operate properly.

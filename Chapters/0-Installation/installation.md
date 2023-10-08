## Introduction


### Objectives and scope of the tutorial

The LLVM Project is a collection of compiler and toolchain technologies. Out of
this collection can be found numerous sub-projects such as the "LLVM Core" libraries
providing a modern source-and-target independent optimiser along with code-generation
support for many different CPUs. "Clang" is the LLVM-native C/C++/Obj-C compiler
which first goal is to deliver fast compiles as well as useful errors and warning
messages.

The "LLVM Core" libraries are built around a well-specified code representation
known as the LLVM Intermediate Representation \(or IR\). This IR can then
be used to perform optimisations and generate target-specific machine code. It is
therefore possible to use LLVM as an optimiser and code generator for any programming
language.

In the scope of Pharo, LLVM could be used as an optimiser or machine code generator
but we have to understand how to perform a translation as an abstract interpretation
of the Pharo VM instructions.

Starting from the beginning, the objectives of the following tutorial are the
following:
- How to generate LLVM Intermediate Representation?
- How to use this Intermediate Representation to generate machine code?


One important note is that the "LLVM Core" libraries are written in C++ and since
it is not much of use from a Pharo point of view, we will use the "LLVM C bindings".

### Installation


In order to install LLVM on your personal computer, several options are possible.
It is possible to use the source code and build the LLVM libraries yourself. In our
case, the easiest way is to use `brew` and the installation it provides. It can
be done using the following command:

```language=bash
$ brew install llvm
```


You will now need to add the paths to your `PATH` adding the following lines
to your `.bash_profile`:

```language=bash
# LLVM
# ====
# PATHS
export PATH="/usr/local/opt/llvm/bin:$PATH"
export DYLD_LIBRARY_PATH=/usr/local/Cellar/llvm/9.0.0_1/lib:$DYLD_LIBRARY_PATH
# FLAGS
export LDFLAGS="-L/usr/local/opt/llvm/lib"
export CPPFLAGS="-I/usr/local/opt/llvm/include"
export LDFLAGS="-L/usr/local/opt/llvm/lib -Wl,-rpath,/usr/local/opt/llvm/lib"
```


!!note If you have XCode installed on your computer, chances are you might need to switch your XCode command line tools
```language=bash
$ sudo xcode-select --switch /Library/Developer/CommandLineTools/
```


You should now have a working LLVM installation!

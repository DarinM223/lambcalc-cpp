lambcalc-cpp
============

A compiler for a variant of lambda calculus extended with integers, if expressions, and binary operator expressions to LLVM.

It is similar to these projects in [Smalltalk](https://github.com/DarinM223/lambcalc-smalltalk) and [Haskell](https://github.com/DarinM223/lambcalc), except that:

* It uses the C++ LLVM API directly instead of writing a textual representation.
* It uses LLVM's JIT support to provide a JIT compiled REPL instead of compiling files to object code.
* It uses a manual recursive descent parser with Pratt parsing for parsing operators instead of a parser combinator library.
* It uses worklists with a heap-allocated stack for most passes for stack safety.
* Stack safe ANF conversion implemented by first writing down the recursive ANF
  conversion algorithm in Standard ML, then CPS converting it and
  defunctionalizing the result. The defunctionalized state machine is then
  translated into C++ as a loop that mutates outside variables which serve as
  parameters for a tail call. The defunctionalization process is written out
  step by step in the [Standard ML file](./src/anf-defunctionalization.sml).

Building
--------

To build the project, run:

```
cmake .
make
```

LLVM 19 is required to be installed and accessible in the PATH, and the C++ compiler must support C++23.

Once the files have finished compiling, to run the JIT REPL, run `./lambcalc`. To run the unit tests, run `ctest`.

To run cppcheck, run:

```
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
cppcheck -igoogletest-prefix --suppress='*:googletest-prefix/*' --enable=all --suppress=missingInclude --suppress=missingIncludeSystem --suppress=useStlAlgorithm --suppress=duplInheritedMember --project=compile_commands.json
```
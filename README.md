# Gale

Gale is a research project I created for the purpose of investigating language implementation. The project is divided into several subprojects (roughly corresponding to compiler pipeline stages):

- `parser` is the parser for Gale. It uses `nom` to parse `.gale` files into the [ATerm format](https://github.com/MDBijman/aterms), an existing format I implemented in Rust.
- `checker` contains the typechecker. The typechecking logic is written in the [Tecs DSL](https://github.com/MDBijman/tecs), a DSL I created for this project.
- `compiler` contains several transformation passes that turn typechecked Gale terms into VM bytecode. These transformation passes are written in the [Ters DSL](https://github.com/MDBijman/ters), another DSL I created for this project.
- `dataflow` contains a small (wip) library for implementing dataflow analyses.
- `pipeline` is a small project that combines all of these stages into a single binary.
- `utils` contains some utilities for working with the compiler and VM, such as dissassembling and printing generated JIT code.
- `tests` contains e2e tests and a small test runner written in python.

I am currently reworking the virtual machine to allow for `dialects` (inspired by [MLIR](https://mlir.llvm.org/docs/Dialects/)) such that it can be extended to more succintly support different kind of programming languages.

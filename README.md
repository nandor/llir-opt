# LLIR Optimiser

The LLIR optimiser performs low-level cross-language optimisations on a
low-level IR which can be easily generated from various compilers.

## Installation

The LLIR optimiser can be set up using ```opam```:

```
opam switch create llir \
  --repositories=llir=git+https://github.com/nandor/llir-opam-repository \
  --empty
opam update
opam install ocaml-variants.4.07.1+llir
```

The repository includes compatible packages.

## Development Setup

The opt, llvm, clang and ocaml projects should be checked out in the following folders:

```
$PREFIX
├─ llvm               https://github.com/nandor/llir-llvm
├─ ocaml              https://github.com/nandor/llir-ocaml
├─ opt                https://github.com/nandor/llir-opt
├─ musl               https://github.com/nandor/llir-musl (Linux only)
└─ dist               install prefix
```
`PREFIX` and `PATH` should be set in the shell's init script.

### llvm and clang

To generate LLIR using LLVM, the llvm-llir and clang-llir forks are required. To build:

```
mkdir $PREFIX/llvm/MinSizeRel
cd $PREFIX/llvm/MinSizeRel
cmake ../llvm                                     \
  -G Ninja                                        \
  -DCMAKE_BUILD_TYPE=MinSizeRel                   \
  -DCMAKE_INSTALL_PREFIX=$PREFIX/dist             \
  -DLLVM_TARGETS_TO_BUILD="X86;LLIR"              \
  -DLLVM_ENABLE_DUMP=ON                           \
  -DLLVM_ENABLE_BINDINGS=OFF                      \
  -DLLVM_ENABLE_OCAMLDOC=OFF                      \
  -DDEFAULT_SYSROOT=$PREFIX/dist/musl             \
  -DLLVM_ENABLE_PROJECTS=clang
ninja
ninja install
```

### opt

Debug builds are configured as follows:
```
mkdir $PREFIX/opt/Debug
cd $PREFIX/opt/Debug
cmake ..                                  \
  -GNinja                                 \
  -DCMAKE_BUILD_TYPE=Debug                \
  -DLLVM_DIR=$PREFIX/dist/lib/cmake/llvm  \
  -DCMAKE_INSTALL_PREFIX=$PREFIX/dist
ninja
ninja install
```

A release build can be configured using `-DCMAKE_BUILD_TYPE=Release`.

### musl

The musl implementation of libc is required on Linux:

```
./configure                  \
  --prefix=$PREFIX/dist/musl \
  --enable-wrapper=all
make
make install
```

When musl is used, ```$PREFIX/dist/musl/bin``` must be added to $PATH.
The las command installs a link to the musl dynamic loader to `/lib/ld-musl-x86_64.so.1`.

### ocaml

To generate LLIR from OCaml, the ocaml-llir fork is required and a symbolic link
to the llvm fork must be added to `$PATH` under the `llir-gcc` alias. The OCaml
compiler can be built using the following commands:

```
cd $PREFIX/ocaml
export PATH=$PATH:$PREFIX/dist/bin
./configure                        \
  --with-llir O0                   \
  --prefix $PREFIX/dist            \
  -no-debugger                     \
  -no-instrumented-runtime         \
  -no-debug-runtime                \
  -no-graph                        \
  -fPIC                            \
  -flambda                         \
  -no-cfi
make world.opt
make install
```

## Adding a new pass

A new pass can be added by creating a header (`simple_pass.h`) and source file
(`simple_pass.cpp`) in the `passes` directory. The pass should inherit from the
`Pass` class in `simple_pass.h`:

```
#pragma once

#include "core/pass.h"

class Func;

class SimplePass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  SimplePass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;
};
```

Methods should be implemented in `simple_pass.cpp`:
```
#include "passes/simple_pass.h"

const char *SimplePass::kPassID = "simple-pass";

void SimplePass::Run(Prog *prog)
{
  // Insert cross-language optimisation here
}

const char *SimplePass::GetPassName() const
{
  return "Simple Pass";
}
```

`simple_pass.cpp` should be added to the passes library in `passes/CMakeLists.txt`
and registered with the `PassRegistry` in `tools/opt/opt.cpp` by adding:

```
registry.Register<SimplePass>();
```

Optionally, a pass can be added by default to an optimisation level. Otherwise,
individual passes can be executed using the `-passes=` flag:

```
llir-opt input.llir -passes=simple-pass,dead-code-elim -o=output.S
```

## Testing

To run the compilation tests bundled with the project:

```
cd $PREFIX/opt/Debug
ninja
PATH=$PREFIX/dist/bin:$PATH ../test.py
```

Unit tests require googletest to be installed:

```
cd $PREFIX/opt/Debug
ninja test
```
